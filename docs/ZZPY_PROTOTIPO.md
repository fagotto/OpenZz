# ZZPy: Python ridotto con sintassi estensibile

26 settembre 2026. Prototipo eseguibile per verificare l'idea:

```text
Python ridotto + import di sintassi
  → lexer del sottoinsieme
  → grammatica e parsing realmente eseguiti da ZZ
  → IR strutturata
  → AST Python standard
  → file Python eseguibile senza ZZ
```

## Cosa dimostra

Un programma importa definizioni di nuovi costrutti e li usa nelle righe
successive. Le estensioni possono contenere espressioni e blocchi annidati.
L'interprete Python resta invariato: riceve esclusivamente Python standard.

Non è una sostituzione testuale di parole e non è `ast.parse` applicato al
sorgente esteso: la grammatica host è in `tools/zzpy/base.zz`, e le produzioni
aggiunte dai moduli vengono installate nel parser ZZ durante la traduzione.
`ast` interviene dopo il parsing, oltre che nella decodifica di singoli letterali.

## Avvio rapido

Dalla radice del repository, dopo una normale build in `build/`:

```sh
python3 tools/zzpy/zzpy.py \
  --zz build/src/ozz \
  examples/zzpy/dynamic.zzpy \
  -o /tmp/dynamic.py
python3 /tmp/dynamic.py
```

Nel workspace usato per lo sviluppo, dalla radice `OpenZz` il binario è invece
`../build-portable/src/ozz`. Sono necessari Python 3.9+ e un binario ZZ compilato;
non sono richiesti pacchetti Python esterni. `--syntax-dir` seleziona la directory
dei moduli; il default è `syntax/` accanto al file sorgente. Senza `-o`, il codice
Python viene scritto su stdout dopo il completamento della traduzione.

Il risultato dell'esempio è:

```text
initial 14
15
17
done 17
```

Il file `examples/zzpy/dynamic.py` è l'output generato dell'esempio. La traduzione
non lo esegue: la seconda riga di comando è una scelta distinta ed esplicita.

## Il programma esteso

```text
total = 2 + 3 * 4
import syntax control version "1"

unless total == 0:
    print("initial", total)

until total >= 17:
    total = total + 1
    unless total == 16:
        print(total)

print("done", total)
```

`unless` viene espanso in `if not ...`; `until` in `while not ...`, con verifica
della condizione **prima** di ciascuna iterazione. Non è un ciclo do/until.
L'assegnazione precedente all'import viene parsata prima dell'installazione delle
nuove regole. Usare un'estensione prima dell'import causa un errore, anche quando
il medesimo file la importa più avanti.

## Definire un'estensione senza cambiare il frontend

I moduli sono file JSON dichiarativi, per esempio `control.zzpy.json`:

```json
{
  "format": 1,
  "name": "control",
  "version": "1",
  "rules": [
    {
      "name": "unless",
      "pattern": [
        "unless",
        {"capture": "condition", "type": "expr"},
        ":",
        {"capture": "body", "type": "suite"}
      ],
      "expand": [
        "if",
        ["not", {"capture": "condition"}],
        {"capture": "body"}
      ]
    }
  ]
}
```

Il modulo incluso contiene anche `until`. `pattern` diventa una produzione
`/py_body -> ...` di ZZ. Le azioni generate dal frontend costruiscono soltanto
una IR JSON, con riferimenti opachi ai valori dei token e alle catture. `expand`
è un template AST verificato per categoria: espressione, nome, blocco, istruzione.
Un blocco non può essere inserito al posto di un'espressione.

Il compilatore non contiene rami speciali per le parole `unless` o `until`.
I test aggiungono moduli indipendenti per `when`, `display` e `increase`, compresa
un'assegnazione che usa una cattura di tipo `name`.

Le catture disponibili sono `expr`, `name`, `suite`. Ogni pattern inizia con
una parola nuova; una cattura `suite` è ammessa soltanto alla fine, preceduta da
`:`. Le forme senza blocco terminano a fine riga. Una nuova parola letterale
viene riservata dal punto dell'import fino alla fine del file.

I template possono usare gli operatori del sottoinsieme, `if`, `while`, `assign`,
`expr`, `pass`, `call`, `constant` e `global`; gli elenchi espliciti sono
`{"args": [...]}` e `{"suite": [...]}`. `global` indica un nome Python da
risolvere nel normale ambiente di esecuzione, non un accesso eseguito dal traduttore.
Non sono disponibili snippet Python/ZZ arbitrari o callback native nei moduli.

## Perimetro del Python ridotto

Supportato:

- assegnazione a un nome, istruzioni espressione, chiamate a funzioni per nome;
- interi decimali, stringhe semplici, `True`, `False`, `None`;
- `+`, `-`, `*`, `/`, `//`, `%`, meno unario, parentesi;
- confronti singoli, `not`, `and`, `or`, con precedenze e short-circuit Python;
- blocchi `if` senza `else`, `while`, `pass`;
- indentazione a spazi, commenti e righe vuote;
- import di sintassi esclusivamente al livello principale, anche dopo blocchi.

Non supportato: funzioni/classi definite nel sorgente, `for`, `else`, `return`,
accessi ad attributi/indici, collezioni letterali, f-string, stringhe triple,
float, confronti concatenati, espressioni multilinea, import Python ordinari,
annotazioni e istruzioni separate da punto e virgola.

Identificatori ASCII; le stringhe possono contenere Unicode. Il file deve essere
UTF-8 con terminatori LF. I tab vengono rifiutati. Un sottoinsieme esplicito evita
di presentare come compatibilità Python completa un insieme parziale di regole.

Gli interi conservano la semantica Python: **non** sono forzati a i64. Le variabili
non devono essere dichiarate a ZZ e un nome inesistente può produrre un normale
`NameError` quando si esegue il file generato. Accettazione sintattica e correttezza
applicativa rimangono proprietà separate.

## Architettura e rapporto con il lavoro precedente

Questo esperimento usa il **motore grammaticale storico**, avviato in un processo
separato per ciascuna traduzione. Il profilo `ozz --checked` e la sua API C restano
invariati: non supportano ancora produzioni generali sufficienti per Python.
Non si dichiara che il vecchio runtime sia diventato rientrante o sanitizer-clean.

Il frontend svolge questi compiti:

1. scanner ristretto per indentazione, operatori, nomi e letterali;
2. controllo dei moduli e conversione delle dichiarazioni in produzioni ZZ;
3. trasmissione dei token, con valori conservati fuori dal sorgente ZZ;
4. verifica dell'esito del processo e conversione della IR in AST Python;
5. `compile` dell'AST e del sorgente emesso, senza `exec` del programma;
6. pubblicazione del file solo dopo il successo dell'intera traduzione.

L'import è gestito dal frontend come metadirettiva, non come normale import
Python. La verifica preliminare del modulo impedisce conflitti di parole iniziali,
versioni incompatibili e template malformati. Eventuali ambiguità più profonde
rimangono errori del parser ZZ quando riconosce il programma.

Ogni traduzione parte da uno stato nuovo. Non ci sono estensioni residue fra due
chiamate. Le versioni sono confrontate con i metadati del file locale; non sono
un lock crittografico sul contenuto, né comportano download o risoluzione di pacchetti.

Questo è un ponte sperimentale verso un futuro parser contestuale generale con
le garanzie del profilo controllato. Dimostra l'idea usando davvero ZZ senza
simulare in Python il riconoscimento dei costrutti estesi.

## Errori, effetti e limiti

- La traduzione non esegue chiamate, cicli o istruzioni del programma destinazione.
- I dati delle stringhe non vengono interpolati come codice ZZ: vengono riferiti
  mediante chiavi generate dal frontend.
- Il processo ignora `ZZ_DEFAULT_INCLUDE_FILE` e `ZZ_INCLUDES` ereditati, per evitare
  inclusioni implicite esterne alla grammatica dichiarata.
- Il processo separato isola lo stato, ma **non è un sandbox di sicurezza**.
- Un fallimento dopo aver parsato una parte del sorgente non pubblica output parziale.
  Con `-o`, un file preesistente resta intatto in caso di errore; il successo usa
  sostituzione atomica nello stesso filesystem.
- Le diagnostiche ZZ sono associate alla riga sorgente quando disponibile.
  Non è ancora presente una source map completa dei traceback del Python generato.
- I template non hanno ancora un sistema generale di igiene. Le estensioni incluse
  non introducono temporanei. Un template che duplica un'espressione può duplicarne
  gli effetti a runtime: l'autore della trasformazione ne è responsabile.

Budget iniziali: 32 KiB sorgente/modulo, 2048 token normalizzati, 32 regole totali,
profondità 24 per indentazione/parentesi/template, letterali da 512 caratteri,
profondità 128 per la IR/espansione, 16384 unità di lavoro AST e 2 MiB di output. Il processo ZZ ha timeout di 10 secondi.
Il controllo del volume di stdout/stderr avviene dopo il termine del processo,
non costituisce una quota di disco imposta dal sistema operativo. Questi sono
limiti del prototipo, non caratteristiche intrinseche di ZZ o Python.

## Verifica

`make check` include `zzpy.sh`, che esegue la suite Python end-to-end se è disponibile
Python 3.9+. Il compilatore C continua a funzionare senza Python; il solo test ZZPy
viene marcato SKIP in sua assenza. I runner CI devono avere Python per eseguire la
prova invece di saltarla.

Risultati locali: **22/22 test Python**, **24/24 test Automake**, build statica
**23 passati e 1 skip** previsto per il modulo dinamico; `make distcheck` passato.
È verificato anche lo skip del solo test Python quando l'interprete non è disponibile.

La suite copre confronto con Python ordinario, moduli nuovi definiti dai test,
ordine degli import, isolamento fra traduzioni, tipi dei template, precedenze,
short-circuit, singola valutazione della condizione, stringhe ostili, indentazione,
versioni, assenza di esecuzione durante la traduzione, file di output preservato
su errore e budget di espansione per template duplicanti.

## Prossima decisione tecnica

Dopo questa prova, generalizzare le produzioni e l'IR del profilo controllato per
poter ospitare la stessa grammatica senza appoggiarsi al motore globale storico.
Solo dopo ampliare il sottoinsieme Python e introdurre definizioni sintattiche
inline, macro con temporanei igienici e source map complete.

Per il C, le decisioni sulla fase precedente al preprocessing sono conservate
separatamente in `C_PRIMA_DEL_PREPROCESSORE.md`.

Riferimento per il backend: [AST Python](https://docs.python.org/3/library/ast.html).
