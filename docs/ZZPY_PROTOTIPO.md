# ZZPy nativo: preprocessore C con azioni ZZ componibili

26 settembre 2026. Questo prototipo sostituisce come direzione di sviluppo il
frontend Python con moduli JSON. La prova precedente resta disponibile in
`tools/zzpy/` e documentata in `ZZPY_LEGACY.md`, con i relativi test di regressione.

## Architettura implementata

```text
sorgente Python ridotto + importazioni + definizioni ZZ
  → zzpy, eseguibile C collegato a libozz
  → normalizzazione lessicale di indentazione e letterali
  → grammatica di base e metagrammatica native ZZ
  → azioni ZZ che richiamano altre produzioni
  → frammenti Python e blocchi con indentazione strutturata
  → Python standard
```

Non viene avviato un interprete Python durante la traduzione. La grammatica è
`tools/zzpy-native/base.zz`, incorporata in `src/zzpy_base.h`: l'eseguibile
installato non dipende dal checkout. `embed.py` serve solo a rigenerare l'header
quando si modifica la grammatica; i test verificano che i due file coincidano.

La libreria usata è il motore storico, non `--checked`. Il profilo controllato
rimane un esperimento separato: i suoi rollback e budget non proteggono le
azioni native. Il driver C non contiene casi speciali per `until` o `tozero`.

## Avvio

Dopo `configure` e `make`, dalla directory `examples/zzpy-native`:

```sh
/path/to/build/src/zzpy dynamic.zzpy -o /tmp/dynamic.py
python3 /tmp/dynamic.py
```

Oppure da qualsiasi directory:

```sh
/path/to/zzpy --grammar /path/to/grammar.zz program.zzpy -o program.py
```

`make install` installa `zzpy` accanto a `ozz`. Python è necessario per eseguire
il risultato e per i test di equivalenza, non per compilare/usare il frontend C.

## Importazioni e definizioni nel sorgente

```python
import syntax "control.zz"
```

Il percorso è relativo alla directory di lavoro, oppure assoluto. La forma
`import syntax` usa `grammar.zz` nella directory di lavoro. Non c'è caricamento
implicito: serve un'importazione oppure `--grammar`. Non ci sono più versioni
JSON o un gestore di pacchetti. Un import nativo viene eseguito ogni volta.

Entrambe le forme sono produzioni `stat` nella grammatica di base, che invocano
il `/include` originale. Il lexer distingue le stringhe di import dalle stringhe
Python, ma caricamento e riconoscimento della direttiva sono responsabilità di ZZ.
I costrutti precedenti all'import sono tradotti prima di caricare nuove regole.
Le variabili d'ambiente di autocaricamento del CLI `ozz` non vengono usate.

Le definizioni possono stare direttamente nel programma:

```text
syntax zz {
    /p_stmt -> "unless" p_expr^c ":" p_suite^b {
        pylower if not ( c ) : b
    }
}

unless ready:
    print("not ready")
```

`syntax zz {` deve iniziare una riga di livello zero. Il driver riconosce questa
isola lessicale e bilancia le graffe; la produzione ZZ usa `$ablock` e `/execute`
per interpretarne il contenuto. Nelle isole si usano commenti ZZ `!!`.
Importazioni e isole sono supportate solo al livello zero.

## Composizione verificata: tozero → until → while

Il file `examples/zzpy-native/grammar.zz` contiene:

```text
/p_stmt -> "until" p_expr^c ":" p_suite^b {
    pylower while not ( c ) : b
}
/p_stmt -> "tozero" ident^id ":" p_suite^b {
    pylower until id == 0 : {
        id = id - 1 __NL
        b
    }
}
```

Il programma:

```python
import syntax

i = 10
v = {}
tozero i:
    v[i] = i
print(i, v)
```

produce:

```python
i = 10
v = {}
while not (i == 0):
    i = i - 1
    v[i] = i
print(i, v)
```

Il test esegue il risultato e verifica `i == 0` e le dieci coppie `v[k] == k`.
Verifica anche due `tozero` annidati. Le funzioni C non conoscono questi costrutti.

`pylower` è esso stesso una regola ZZ:

```text
/stat -> "pylower" p_stmt^s :rreturn
```

Il `:rreturn` nativo restituisce il valore alla chiamata dell'azione esterna.
La regola consente di scrivere un costrutto nel corpo dell'azione e usarne il
risultato come espansione, senza aggiungere l'intero linguaggio Python a `$arg`
(e quindi senza introdurre ambiguità con le espressioni del metalinguaggio ZZ).

## Contratto delle azioni

- `p_expr` restituisce un frammento di espressione Python (`qstring`).
- `p_stmt` e `p_suite` restituiscono un valore `pyblock`, che contiene righe
  relative al proprio livello di indentazione.
- Un blocco catturato può comparire come istruzione o suite nelle azioni.
- Nei corpi ZZ, `__NL` termina un'istruzione Python semplice; le graffe delimitano
  una suite. Nel sorgente Python questi marcatori sono ricavati dal lexer.
- Gli interi scritti nelle azioni usano il lexer storico; gli interi del sorgente
  sono conservati come testo, fino a 100 cifre. Le stringhe Python nel sorgente
  vengono conservate con virgolette ed escape; per costruirne una nell'azione
  bisogna fornire un frammento con le virgolette Python, per esempio `"\"hello\""`.
- `pycat`, `pyline`, `pyassign`, `pysequence`, `pycompound` sono primitive generiche
  C. `pycat` conserva esattamente il testo, evitando la conversione implicita
  dell'operatore storico `&`. `pycompound` indenta ogni riga del blocco.

È un'emissione per frammenti tipizzati come espressione/blocco, non un AST Python
completo né un sistema di template igienici. Le estensioni devono rispettare
precedenze, numero di valutazioni e nomi introdotti. `until` racchiude la condizione
in parentesi; `tozero` non introduce temporanei.

Le azioni sono analizzate con la grammatica attiva quando vengono eseguite.
Una prova definisce `outer` prima di `inner`, poi ridefinisce `inner` e verifica
che il secondo uso di `outer` cambi espansione. Non promettiamo binding immutabile
alla dichiarazione. La semantica delle variabili ZZ resta quella originale.

## Sottoinsieme e limiti

Supportati: assegnazioni a nomi o a un indice, indicizzazione, chiamate posizionali,
interi decimali, stringhe semplici, booleani/None, dizionari e liste vuoti,
`+ - * / // %`, confronto singolo, `not/and/or`, parentesi, `if`, `while`, `pass`,
commenti Python e blocchi indentati a spazi. Le catene booleane non ricevono
parentesi artificiali: i test confrontano anche le chiamate a `__bool__`.

Non è Python completo: niente def/class/for/else, attributi, f-string, float,
confronti concatenati, import Python ordinari, espressioni multilinea o tab.
Gli identificatori che iniziano con `__` sono riservati alla normalizzazione.
Gli escape di stringa supportati sono backslash, virgolette e `nrtbfav`;
Unicode letterale è conservato, gli escape numerici non sono ancora supportati.
Nelle azioni ZZ la divisione intera usa `__floordiv` perché `//` è un commento ZZ.

Sorgente fino a 32 KiB, 4096 token Python, 32 livelli di indentazione/delimitatori,
400 byte per stringa, 100 per identificatore/intero. Frammenti allocati dal driver,
normalizzazione e output hanno limiti di 2 MiB separati. Questi non limitano
la memoria del motore storico o le grammatiche importate.

I file `.zz` e le isole native sono codice fidato eseguito durante la traduzione:
possono usare le operazioni del motore storico. Non c'è sandbox, timeout interno,
rollback delle azioni o protezione dalla ricorsione di una grammatica arbitraria.
Il solo codice Python ospite viene tradotto senza eseguirlo. Gli errori riportano
file e fine dell'unità di traduzione corrente, oltre alle diagnostiche native ZZ;
una mappa completa delle posizioni rimane da implementare.

L'output generato viene accumulato e pubblicato solo se la traduzione riesce;
`-o` usa un temporaneo nella directory di destinazione e rename. Un errore ZZ
lascia intatto l'output preesistente. Questo non annulla altri effetti di azioni
ZZ scritte dall'utente. Un'estensione può generare Python scorretto: i test
compilano il risultato, il frontend C non avvia Python per validarlo.

## Verifica

`testsuite/zzpy-native-tests.py` esercita il binario reale: import esplicito/default,
configurazione CLI, definizioni inline, ordine, composizione, annidamento,
estensioni nuove, binding dinamico, equivalenza Python e `__bool__`, errori,
assenza di esecuzione del codice ospite e pubblicazione dell'output.
È incluso in `make check`; resta anche la suite del prototipo precedente.

Risultati locali macOS ARM64 di questa revisione:

- 20 test end-to-end del frontend nativo superati.
- Suite Automake completa: 25/25 superati; nella build statica 24 superati e
  uno skip previsto per il modulo dinamico storico.
- `make distcheck` superato, incluso install/uninstall e test dal pacchetto.
- Installazione statica in staging e traduzione con `PATH=/nonexistent`:
  riuscita, senza Python o altri programmi esterni disponibili al frontend.
- Output dell'esempio installato eseguito con Python e confrontato nei test.
- Sorgente C del frontend verificato con `-Wall -Wextra -Wpedantic -Werror`,
  disabilitando solo `-Wstrict-prototypes` per le dichiarazioni legacy negli
  header della libreria. Queste prove non certificano assenza di problemi
  nel motore storico né compatibilità con Python completo.
