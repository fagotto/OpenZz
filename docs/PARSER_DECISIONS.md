# Disambiguazione e terminali contestuali: prove sul parser nativo

## Risultato

Sono state aggiunte 22 piccole prove indipendenti da Python, eseguite in
processi separati. Usano `zz_parse_tokens`, grammatiche native e callback C
che contano le azioni. Verificano accettazione, rifiuto, conteggi degli effetti
e, per i casi negativi, la categoria della diagnostica.

Questo incremento caratterizza il comportamento esistente: non modifica
l'algoritmo di riconoscimento e non introduce direttive nuove. I rifiuti
attesi passano il test quando il limite viene riprodotto correttamente;
non sono casi di sintassi già supportata.

La conclusione è che conviene mantenere token atomici e fattorizzare le
grammatiche dove possibile. Per terminali contestuali generali occorre
ancora una capacità del parser: la priorità attuale dei letterali non
fornisce un ripiego sugli identificatori quando il seguito fallisce.

## Come riprodurre

Il normale `make check` esegue `parser-decisions.sh`, che avvia il programma
C `parser-decisions` una volta per scenario. Non richiede Python.
Da una directory di build configurata:

```sh
make check
cd testsuite
./parser-decisions no-fallback
./parser-decisions late-distinction
./parser-decisions early-effect
```

Ogni scenario restituisce zero se l'esito coincide con quello atteso,
anche quando quell'esito è un rifiuto del parser. Il sorgente completo,
con input, grammatica e conteggi attesi, è `testsuite/parser-decisions.c`.
Il reader di prova usa il lexer ZZ come ausilio e ha un limite di 256 letture;
non è un lexer generale nuovo. La modalità con operatori atomici e quella
con classificazione contestuale sono esplicitamente selezionate per scenario.

## Parole contestuali

```zz
/stat -> word ident^name { record 1 }
/stat -> word select ident^name ":" { record 2 }
```

`record` è una callback C del test che incrementa il contatore indicato.
Con la prima regola soltanto, `word select;` viene accettato. Dopo aver
aggiunto la seconda, `word select x:;` è accettato ma `word select;` viene
rifiutato: dopo `select` il parser si aspetta un identificatore.
Invertire l'ordine di definizione delle regole non cambia questo risultato.

La parola non è riservata globalmente: se la seconda regola inizia con
`command`, `word select;` continua a funzionare. La priorità opera sulle
transizioni disponibili nello stato corrente.

Le prove `dynamic-context` e `dynamic-fallback` aggiungono la seconda
regola dall'azione di `enable` durante lo stesso flusso di input. Il nuovo
costrutto diventa utilizzabile subito; contemporaneamente il caso breve
perde la possibilità di interpretare `select` come nome. La dinamicità è
preservata, ma rende necessario considerare il cambiamento della grammatica
in qualsiasi futura cache di riconoscimento.

### Una soluzione limitata già disponibile

Le prove `typed-*` registrano il tag applicativo `selection` e usano:

```zz
/stat -> word ident^name { record 1 }
/stat -> word selection^keyword ident^name ":" { record 2 }
```

Il lexer di prova restituisce `selection` per `select` solo se i due token
successivi sono `ident` e `":"`; altrimenti restituisce `ident`.
Entrambe le forme valide vengono accettate e quella incompleta è rifiutata.

È un esempio di classificazione esterna con un seguito limitato, non di
ripiego automatico del parser. La politica del lexer è specifica di questa
piccola grammatica: applicarla indiscriminatamente ad altri contesti sarebbe
errato. Non dimostra una soluzione per estensioni arbitrarie, nesting o
predicati che dipendono dallo stato sintattico.

## Disambiguazione delle riduzioni

```zz
/left -> ident^x { record 1 }
/right -> ident^x { record 2 }
/stat -> pick left^x ":" { record 3 }
/stat -> pick right^x ":" { record 3 }
```

`pick x:;` produce `Ambiguous syntax (2)`. Nessuna delle tre azioni viene
eseguita. Se il secondo ramo richiede `+` invece di `:`, lo stesso input
sceglie il primo ramo: l'azione di `left` e quella finale vengono eseguite
una volta, quella di `right` mai.

Un caso più significativo distingue i rami soltanto dopo i due punti:

```zz
/stat -> pick left^x ":" a { record 3 }
/stat -> pick right^x ":" b { record 3 }
```

Anche `pick x: a;` fallisce con ambiguità. I due enunciati completi sono
distinguibili, ma la scelta fra le riduzioni avviene prima di leggere `a`.
Per questo è più preciso parlare di conflitto del riconoscitore in quel
punto che concludere che l'intera grammatica sia necessariamente ambigua.

La fattorizzazione risolve questo esempio:

```zz
/item -> ident^x { record 1 }
/stat -> pick item^x ":" a { record 2 }
/stat -> pick item^x ":" b { record 3 }
```

I rami condividono la stessa riduzione iniziale. Il test `factored` accetta
`pick x: a;` con un'esecuzione delle azioni 1 e 2. Il caso `same-prefix`
e il suo secondo ramo verificano anche alternative che condividono
direttamente i primi token senza riduzioni distinte.

## Operatori atomici

L'esempio `split-operator` riproduce il conflitto tra assegnamento `=` e
uguaglianza `==`, con riduzioni separate per target e atomo. Nel lexer
nativo `==` è una sequenza: il primo `=` non basta a scegliere la riduzione.

`atomic-assignment` e `atomic-equality` mappano gli operatori a due token
letterali distinti, `ASSIGN` ed `EQ`, e accettano entrambi gli input.
`operator-tags` verifica anche la versione con tag applicativi `assignop`
ed `eqop`, evitando di occupare nomi identificatore. I tag operatori devono
essere prodotti dal lexer, non dal testo del programma ospite.

È una soluzione del conflitto misurato, non una garanzia che token atomici
rendano qualsiasi grammatica riconoscibile da ZZ.

## Momento di esecuzione delle azioni

```zz
/piece -> ident^x { record 1 }
/stat -> begin piece^x ":" int^value { record 2 }
```

- `begin x: 7;`: le azioni 1 e 2 vengono eseguite una volta ciascuna.
- `begin x: wrong;`: la traduzione fallisce, ma l'azione 1 è già avvenuta.
  L'azione 2 non viene eseguita.

Le prove escludono l'esecuzione dei rami concorrenti nei conflitti esaminati;
non dimostrano transazionalità di un intero enunciato. Non sarebbe corretto
promettere che un input rifiutato non abbia effetti.

Lettura del codice coerente con gli esperimenti:

- `try_shift` in `src/parse.c` preferisce letterali (livello 3) ai tag
  (livello 2) e al jolly (livello 1); i parametri nativi hanno priorità 4.
- `lr_loop` segnala il conflitto quando ci sono più candidati nella workarea.
  Solo con un candidato chiama `lr_reduce`.
- `lr_reduce` esegue l'azione immediatamente: la selezione locale di una
  riduzione non equivale alla validazione dell'intero enunciato.

## Inventario degli scenari

| Gruppo | Scenari | Verifica |
| --- | --- | --- |
| Terminali | name, literal-wins, no-fallback, reverse-order, other-context | Priorità, assenza di ripiego, contesto e ordine |
| Prefisso condiviso | same-prefix, same-prefix-second | Entrambi i rami restano utilizzabili |
| Mutazione della grammatica | dynamic-context, dynamic-fallback | Nuove regole attive nello stesso flusso |
| Riduzioni | reduce-conflict, distinct-follow, late-distinction, factored | Conflitto locale e fattorizzazione |
| Azioni | early-effect, complete-effect | Effetti anticipati e conteggi esatti |
| Operatori | split-operator, atomic-assignment, atomic-equality, operator-tags | Separazione dei token |
| Classificazione esterna | typed-name, typed-keyword, typed-incomplete | Politica contestuale limitata nel lexer |

## Validazione di questo incremento

Su macOS, `make check`: 21/21 test Automake; il nuovo test comprende i
22 scenari della tabella. Build statica: 20 passati e uno skip previsto
per il modulo dinamico. `make distcheck` superato, inclusa la distribuzione
del nuovo test e del documento. Questi sono risultati locali; i risultati
CI delle altre piattaforme si consultano sulla PR #3.

## Proposta per il prossimo incremento del core

Prima di una sintassi slash pubblica, sperimentare un'API C generica di
terminali contestuali e predicati di riconoscimento con questi vincoli:

1. Il predicato riceve token immutabili e un seguito limitato esplicito;
   restituisce solo corrispondenza/non corrispondenza/errore. Nessuna azione
   ZZ, I/O, modifica di parametri o grammatica durante il riconoscimento.
   È un contratto per il callback C, non una sandbox tecnicamente garantita.
2. I token esaminati anticipatamente devono essere conservati e riutilizzati:
   non si può richiamare un reader con effetti per simulare un ritorno indietro.
   Durata dei payload e limiti del buffer devono essere definiti prima dell'API.
3. La politica fra candidato contestuale e identificatore deve essere esplicita.
   Un predicato locale può risolvere i casi con seguito limitato; non risolve
   automaticamente il caso `late-distinction` per grammatiche arbitrarie.
4. Le azioni ordinarie restano separate dai predicati. Se si introduce una
   ricerca su più derivazioni, nessuna azione dei rami di prova deve essere
   eseguita. Non basta spostare una callback dopo una scelta già impossibile.
5. Le azioni native selezionate devono continuare a poter definire e usare
   subito altre regole. Le cache eventualmente introdotte devono distinguere
   le revisioni della grammatica; anche eventuali classificazioni anticipate
   dipendenti dalla grammatica richiedono invalidazione.
6. Conservare gli scenari attuali come caratterizzazione del modo storico;
   aggiungere test distinti del nuovo modo, comprese priorità concorrenti,
   errori del predicato, limite del seguito ed esecuzione singola delle azioni.

La fattorizzazione e la tokenizzazione appartengono alla progettazione della
grammatica/lexer ospite. Gestione dei candidati contestuali, buffering del
seguito, invalidazione e contratto di esecuzione sono capacità riusabili
che devono stare in ZZ. Le azioni C specifiche del linguaggio costruiscono
nodi e verificano la semantica; non devono nascondere una seconda logica di
parsing specifica per Python dentro il core.
