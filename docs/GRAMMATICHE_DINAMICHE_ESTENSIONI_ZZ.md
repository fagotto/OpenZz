# Grammatiche dinamiche: applicazioni e proposta di evoluzione di ZZ

Ricerca del 26 settembre 2026. Documento progettuale, basato su fonti primarie
online e sul codice locale di OpenZz. Le direttive qui proposte **non sono
implementate** e gli esempi di estensione sono pseudocodice di una possibile
RFC. Non sono state modificate né la proposta iniziale né l'implementazione C.

## 1. Valutazione

L'idea di ZZ rimane utile: il linguaggio riconoscibile può dipendere dal
programma già letto. La ricerca mostra applicazioni concrete sia alla
costruzione di linguaggi estensibili sia alla generazione vincolata con LLM.
Le due applicazioni condividono una parte del modello, ma hanno esigenze
operative diverse.

La direzione che propongo è conservare il nucleo delle grammatiche evolutive
aggiungendo tre separazioni esplicite:

1. grammatica, ambiente dei simboli e stato della singola analisi;
2. riconoscimento, costruzione dell'AST ed esecuzione degli effetti;
3. proposta di modifica, validazione e attivazione della modifica.

Nuove parole chiave da sole non eliminerebbero buffer vulnerabili, callback
incompatibili o stato globale. Alcune direttive possono essere realizzate
come estensioni ZZ; altre devono esporre funzionalità nuove del runtime C.

## 2. Cosa ho trovato: usi effettivi e sistemi affini

La somiglianza tecnica non implica discendenza da ZZ. Distinguo impieghi
diretti, sistemi con obiettivi affini e lavori sul decoding. I repository
citati sono consultabili, ma non ne ho eseguito gli esperimenti in questa
sessione; non assegno loro una maturità produttiva non verificata.

| Sistema | Uso documentato | Rapporto con la proposta |
|---|---|---|
| **ZZ / TAO / APE** | Compilatori e strumenti di sistema; dichiarazioni e tipi possono aggiungere produzioni. Il manuale cita anche debugger e descrizione della macchina. | Uso storico diretto di ZZ. [Manuale degli autori](https://catseye.tc/modules/OpenZz/doc/zzdoc.html). |
| **Zzrk** | Avventura testuale scritta in ZZ: disponibilità di parole e comandi dipendente dallo stato del gioco. | Dimostrazione diretta di grammatica come parte dello stato applicativo, su scala didattica. [Descrizione dell'autore](https://catseye.tc/view/zzrk/README.markdown). |
| **SugarJ** | Librerie importabili estendono la sintassi Java; esempi comprendono XML, coppie e closure, con trasformazioni verso il linguaggio di base. | Riferimento per moduli sintattici e traduzione AST. [Paper degli autori](https://www.cs.cmu.edu/~ckaestne/pdf/oopsla_sugarj.pdf), [progetto](https://www.informatik.uni-marburg.de/~seba/projects/sugarj/). |
| **APEG** | Grammatiche PEG adattabili; implementazione e confronto di un parser SugarJ con estensioni XML, closure e coppie. | Precedente per rappresentare l'adattamento nel formalismo e misurare separatamente modifica e parsing. Le prestazioni pubblicate riguardano quel prototipo e quei benchmark. [Paper](https://www.llp.dcc.ufmg.br/Publications/Journal2014/2014-scp-leonardo-formal-apeg.pdf). |
| **Iguana** | Grammatiche dipendenti dai dati, nonterminali parametrizzati, vincoli, precedenze e layout; esempio di corrispondenza fra tag XML. | Utile per estendere ZZ con parametri e predicati; non equivale necessariamente ad aggiungere produzioni a runtime. [Progetto](https://iguana-parser.github.io/), [documentazione](https://iguana-parser.github.io/documentation.html), [esempio](https://iguana-parser.github.io/getting_started.html). |
| **PICARD** | Parsing incrementale usato per filtrare continuazioni LLM, applicato a text-to-SQL. | Precedente operativo per analizzare prefissi incompleti e riprendere lo stato. [Paper EMNLP 2021](https://aclanthology.org/2021.emnlp-main.779.pdf), [codice ufficiale](https://github.com/servicenow/picard). |
| **Tree-of-Parsers / ToP** | Parser contestuali modulari organizzati ad albero, con scope e vincoli; sperimentazione su un linguaggio basato su Lua e generazione per un gioco. | Riferimento per gestire più continuazioni contestuali. Le garanzie vanno lette rispetto al linguaggio e ai vincoli formalizzati. [Paper](https://arxiv.org/html/2508.15866v1). |
| **Type-Constrained Code Generation** | Generazione guidata dai tipi, con implementazione sperimentale per TypeScript. | Collegamento particolarmente diretto alla traccia Semantic Guard della proposta. [Paper PLDI 2025](https://arxiv.org/abs/2504.09246), [pacchetto di riproduzione](https://github.com/eth-sri/type-constrained-code-generation). |
| **ChopChop** | Vincoli semantici programmabili su spazi di programmi; casi sperimentali TypeScript ed e-graph. | Riferimento per vincoli oltre la sola appartenenza di un nome all'ambiente. [Paper](https://arxiv.org/abs/2509.00360), [codice ufficiale](https://github.com/large-loris-models/chopchop). |

Questi esempi giustificano l'esplorazione, ma non dimostrano che riutilizzare
OpenZz sia necessariamente più economico o più efficace di partire da un
parser moderno. Questo confronto deve rimanere parte dell'esperimento.

## 3. Il paper citato nel documento iniziale

Ho verificato il riferimento *Decode-Time Grammars: Constrained LLM Generation
over a Refinement Order of Grammar Fragments*, arXiv:2607.18357, e consultato
il [testo HTML primario](https://arxiv.org/html/2607.18357v1).

Il sistema descritto, **gproj**, specializza frammenti grammaticali usando
l'ambiente corrente Γ; le dichiarazioni aggiornano i riferimenti disponibili.
La valutazione comprende TileLang, SQL, P4 e interfacce di strumenti.
La proprietà No-Ghost riguarda l'esclusione dei riferimenti inesistenti,
sotto le ipotesi del modello. Le sezioni 5.4 e 6 separano questa garanzia
dalla compilazione e dalla correttezza funzionale. Per esempio, un operando
esistente può comunque essere quello sbagliato.

I risultati sono degli autori, non riprodotti qui. I campioni e le ablation
sono mirati: non li interpreto come una soluzione generale della generazione
corretta. Nelle pagine e ricerche consultate non ho identificato con certezza
un repository ufficiale scaricabile di gproj; questo limita la verifica
indipendente. Non significa che tale repository non esista.

### Conseguenza progettuale per ZZ — mia proposta

Formalizzerei separatamente le proprietà che intendiamo ottenere:

- **R0:** stringa accettata dalla grammatica;
- **R1:** riferimenti risolti nell'ambiente dichiarato;
- **R2:** tipi coerenti secondo il sistema di tipi implementato;
- **R3:** effetti ammessi secondo il modello di capacità;
- **R4:** esito di compilatore, analizzatore o test esterni.

R1 non implica R2, R2 non implica correttezza dell'algoritmo. Un sistema
potrebbe verificare solo alcune proprietà. L'output dovrebbe elencarle
singolarmente, con versione dell'ambiente e limiti della verifica, evitando
un'etichetta generica «programma verificato».

## 4. Nuove direttive: cosa proporrei e a quale costo

Tutta la sintassi nelle sezioni seguenti è **proposta**, non sintassi ZZ
corrente. I nomi delle direttive sono provvisori; prima vengono i contratti
operativi, poi la loro notazione.

### 4.1 `/patch`: modifiche atomiche e versionate

```text
/patch begin "increment" base "g12"
  ... dichiarazioni di simboli, regole e azioni ...
/patch check
/patch commit
```

Il runtime costruisce una versione candidata. Controlla dipendenze, tipi
delle azioni, compatibilità del profilo grammaticale e test associati.
Attiva la modifica soltanto se tutti i controlli richiesti riescono e la
versione base è ancora quella attesa. In caso contrario mantiene la versione
precedente e restituisce diagnostiche strutturate.

**Correzione di un limite concreto:** oggi `scope.c:insert_rule` può sostituire
immediatamente una regola ed eseguire hook; il pop di uno scope non costituisce
una transazione su ogni effetto avvenuto.

**Lavoro necessario:** `zz_context`, strutture immutabili o copy-on-write,
identificatori stabili e attivazione a confini espliciti. Non prometterei di
annullare scritture su file o callback C arbitrarie: durante la validazione
tali effetti devono essere vietati o rinviati. Anche gli hook devono seguire
il nuovo contratto.

**Limite teorico:** `/patch check` non deve promettere una decisione generale
sull'ambiguità di CFG arbitrarie. Per un primo profilo si possono ammettere
solo forme analizzabili dal validatore scelto, segnalare conflitti noti e
rifiutare le estensioni fuori profilo. Test e fuzzing restano evidenze finite.

### 4.2 `/symbol`, `/slot`: nomi disponibili come concetto esplicito

```text
/symbol total : i64 mutable
/slot int_ref : Symbol<i64> from visible_symbols
```

`int_ref` riconoscerebbe esclusivamente simboli visibili e compatibili con
il tipo atteso, restituendo un'identità stabile. Il tipo di una variabile
sarebbe distinto dal suo spelling e dal suo nome C generato.

ZZ sa già simulare una parte del comportamento aggiungendo una produzione
per ogni variabile: lo abbiamo dimostrato. La novità sarebbe rendere questo
meccanismo un'API interrogabile, con scope, provenienza, invalidazione e
controlli uniformi. L'implementazione potrebbe usare produzioni generate o
un indice dedicato: la scelta va misurata su ambienti di dimensioni crescenti.

Per un repository reale, l'ambiente dovrebbe provenire da compiler/language
service e dipendenze effettive, con un identificatore di snapshot. Un elenco
di nomi ottenuto una volta non è sufficiente dopo modifiche ai file.

### 4.3 `/rule` tipizzate e AST: separare analisi ed effetti

```text
/rule increment : Stmt -> "bump" int_ref^x "by" expr<i64>^n
  => Assign(x, CheckedAdd(Load(x), n))
```

Le azioni costruiscono nodi verificabili; il backend C o l'interprete li
esegue successivamente. `Assign` qui è un costruttore di AST: crearlo non
modifica ancora una variabile del programma eseguito. `CheckedAdd` esprime
una politica aritmetica da implementare, non l'addizione C con overflow
lasciato indefinito.

Proporrei contratti di effetto delle azioni:

```text
/action build_increment effect pure
/action register_symbol effect environment
/action write_output effect external
```

Durante riconoscimento speculativo: azioni pure e modifiche dell'ambiente
private del ramo. Effetti esterni soltanto dopo accettazione e autorizzazione
del livello chiamante. Una funzione C non diventa pura dichiarandola tale:
nel profilo controllato si ammettono primitive predefinite e verificabili;
le estensioni native arbitrarie restano esplicitamente fidate.

Questa separazione serve anche senza LLM: evita output C parziale prima
della scoperta di un errore e rende possibili backend multipli.

### 4.4 `/operator`: precedenza e associatività affidabili

```text
/operator "+" infix precedence 40 associativity left
/operator "*" infix precedence 50 associativity left
```

Nel fork attuale `/prec` ha le azioni commentate. Per la prima implementazione
compilerei queste dichiarazioni in famiglie di nonterminali come quelle del
nostro esempio `atom/term/expr`, mantenendo un sottoinsieme ben definito.

Controlli: arità, firma dei tipi, conflitti e test delle parentesizzazioni.
La priorità di operatori non dovrebbe diventare un modo implicito di scegliere
fra due interpretazioni semanticamente diverse di un'intera istruzione.

Una modalità GLR/GLL con foresta di parsing è una ricerca separata: non basta
modificare la branch che oggi segnala `Ambiguous syntax`. Servono stati
semantici distinti e azioni senza effetti prematuri per ogni alternativa.

### 4.5 `/module`, `/import syntax`: librerie di linguaggio

```text
/module finance version "1"
/export syntax money_literal, payment_stmt
/import syntax finance version "1"
```

Il modulo esporta regole, tipi e costruttori AST dichiarati, con dipendenze
bloccate a una versione. L'import deve essere esplicito, riproducibile e
rifiutabile in caso di conflitto.

Aggiungerei template AST con gestione igienica dei nomi: i temporanei
introdotti da una macro ricevono identità nuove, evitando la cattura delle
variabili del chiamante. Gli scope ZZ esistenti possono essere una base, ma
non forniscono da soli packaging, contratti o igiene.

### 4.6 `/lexmode`, `/layout`: lexer estensibile ma controllato

```text
/lexmode host identifiers unicode strings python_subset
/layout indentation emits NEWLINE INDENT DEDENT
```

Servirebbero a ospitare linguaggi con lessico diverso da quello storico di
ZZ e a incorporare DSL. Per Python, specificare stringhe e indentazione
resterebbe solo una parte della compatibilità.

Il runtime deve conservare stato lessicale, byte incompleti, stack di
indentazione e posizioni sorgente. Ogni modalità deve avere confini di
entrata/uscita e politiche di errore; eviterei callback lessicali arbitrarie
nella prima versione. Una direttiva non può limitarsi a rinominare i token.

### 4.7 `/prefix`, `/expect`, `/checkpoint`: interfaccia incrementale

```text
/prefix feed "bump to"
/expect symbols
/checkpoint save "p17"
```

Queste forme rappresenterebbero API come `feed_bytes`, `status`, `expected`,
`fork_state` e `restore`. Distinguere tre esiti: prefisso impossibile,
prefisso ancora incompleto, programma completo. Aggiungere anche `unknown`
quando un'analisi più forte esaurisce il proprio budget.

Il parser attuale non espone questo contratto. La lista per le diagnostiche
`expected` in `parse.c`, peraltro limitata, non è una maschera pronta per LLM.
Occorre analizzare i prefissi con stato recuperabile e senza azioni esterne.

Un token del tokenizer LLM può contenere parte di un identificatore, più
simboli grammaticali o byte di una sequenza UTF-8. La maschera deve valutare
le continuazioni del tokenizer, non soltanto confrontare parole complete.
Budget esaurito o insieme vuoto devono produrre un risultato esplicito:
non si deve passare silenziosamente a generazione libera.

### 4.8 `/test`, `/explain`, `/budget`: osservabilità e limiti

```text
/test accepts "bump total by 3"
/test rejects "bump unknown by 3"
/explain symbol total
/budget parser_states 10000
/numeric i64 overflow error
```

Le estensioni portano esempi positivi/negativi, versione e provenienza; le
diagnostiche spiegano quale regola o vincolo ha escluso una continuazione.
I budget producono errori recuperabili, mai `exit(0)` dalla libreria.

I test non costituiscono una dimostrazione universale. Il limite sugli stati
può rifiutare programmi validi troppo costosi: questa perdita di completezza
va dichiarata. La politica numerica richiede controlli sia nelle primitive
ZZ sia nel codice emesso dal backend.

## 5. Un esempio integrato delle nuove possibilità

Si supponga che la versione base abbia già `expr<i64>`, `Stmt`, costruttori
AST controllati e una tabella dei simboli contenente `total: i64`.

```text
/patch begin "increment" base "g12"

/slot int_ref : Symbol<i64> from visible_symbols
/rule increment : Stmt -> "bump" int_ref^x "by" expr<i64>^n
  => Assign(x, CheckedAdd(Load(x), n))

/test accepts "bump total by 3"
/test rejects "bump missing by 3"

/patch check
/patch commit
```

**Comportamento desiderato, non implementato:**

1. Il candidato viene verificato su una copia di grammatica e ambiente.
2. Un conflitto o un test negativo non rispettato impediscono l'attivazione.
3. L'attivazione genera `g13`, collegata a `g12` e alla patch.
4. `bump total by 3` produce AST; nessuna assegnazione viene eseguita in parsing.
5. Il backend genera somma controllata e assegnazione.
6. Un decoder collegato può limitare il riferimento ai simboli compatibili.

La stessa infrastruttura permetterebbe altri esperimenti:

- unità fisiche, con tipi dimensionali e conversioni dichiarate;
- API di librerie, con membri e firme estratti dalla versione installata;
- tensori, con shape e layout controllati da un'analisi dedicata;
- protocolli di chiamata, in cui lo stato abilita le operazioni successive;
- DSL locali al modulo, traducibili in un'IR comune.

Queste sono ipotesi di impiego. Membership dei nomi, controllo dimensionale,
shape e stato dei protocolli sono verifiche diverse: ciascuna richiede il
proprio modello e non deriva automaticamente dalla parola `/slot`.

## 6. Strategia di integrazione con i modelli

### Prima opzione: ZZ orchestra frammenti, decoder specializzato applica maschere

Al confine di una dichiarazione o regione, ZZ calcola l'ambiente e il
frammento consentito; un backend di decoding lo applica fino al confine
successivo. AST e aggiornamenti sono convalidati prima di diventare lo stato
per la regione seguente.

È la mia prima scelta sperimentale: permette di confrontare l'utilità dello
stato ZZ senza implementare subito tutta l'integrazione con i tokenizer.
[XGrammar espone API per maschere, accettazione e rollback](https://xgrammar.mlc.ai/docs/latest/api/python/grammar_matcher.html),
con una [guida d'integrazione nei motori](https://xgrammar.mlc.ai/docs/latest/using_xgrammar/engine_integration.html).
Questo non implica che possa eseguire direttamente azioni ZZ o cambiare
arbitrariamente grammatica dentro una regione: serve un adattatore con
confini e contratti espliciti.

### Seconda opzione: parser ZZ completo dentro il decoding

Offre più continuità con le grammatiche evolutive, ma richiede stati isolati,
rollback, lexer incrementale, prova dei candidati senza effetti, cache e
controllo del costo. Cambiando grammatica a metà prefisso occorre specificare
se lo stato corrente rimane valido, va migrato o deve essere rianalizzato.

Cache e snapshot devono identificare almeno grammatica, ambiente, prefisso,
versione del tokenizer e politica dei vincoli. Non basta il nome dello scope.

### Senza accesso al ciclo di decoding

Rimane possibile generare una patch strutturata e validarla prima di
applicarla. È un esperimento diverso dal mascheramento dei token. Il requisito
tecnico per quest'ultimo è un motore controllabile o un'API che esponga i
vincoli richiesti: non si può presumere questa capacità su ogni endpoint LLM.

## 7. Piano per fasi e verifiche di uscita

| Fase | Lavoro | Evidenza richiesta |
|---|---|---|
| 0 — Runtime | Buffer e allocazioni controllate, callback tipizzate, codici di errore; isolamento iniziale per processo. | Regressioni, sanitizer, fuzzing; input malformato non termina il processo chiamante. |
| 1 — Modifiche e IR | `/patch`, versioni, azioni pure, AST minimo, `/symbol` e `/slot`. | Una patch fallita non altera lo stato; snapshot riproducibili; nessun riferimento fuori ambiente nei casi del modello. |
| 2 — Linguaggio | `/operator`, moduli, template igienici; lexer/layout dove richiesto dal caso d'uso. | Precedenze, conflitti fra import, assenza di cattura e posizioni diagnostiche testate. |
| 3 — Generazione | Prima adattatore per frammenti; poi confronto con parser ZZ incrementale. | Misure di validità, costo e dead-end; maschere coerenti con il validatore sul sottoinsieme definito. |
| 4 — Coding agent | Ambiente estratto dal repository, AST/patch validate, compilatore e test esterni. | Benchmark a parità di modello, prompt, budget e task; confronto anche con strumenti esistenti. |

Valuterei almeno quattro condizioni: generazione libera, grammatica statica,
vincoli sui simboli e vincoli sui simboli più tipi. Per la traccia agent,
aggiungerei il confronto fra feedback del compilatore e validazione prima
dell'applicazione della patch.

Metriche: riferimenti inesistenti, compile/test pass rate, errori di tipo,
numero di iterazioni, token totali, latenza p50/p95, memoria, costo di
aggiornamento dell'ambiente, dead-end e frequenza di esaurimento budget.
Una riduzione dei riferimenti inesistenti con test funzionali invariati è
un risultato utile ma distinto dal miglioramento della correttezza.

## 8. Decisione che raccomando

Per il primo prototipo sceglierei **`/patch` + AST puro + `/symbol`/`/slot`**,
con un sottoinsieme numerico tipizzato e backend C. Aggiungerei `/operator`
come prima comodità del linguaggio, perché il comportamento può essere
verificato su esempi piccoli. Il decoding arriverebbe dopo il contratto
incrementale, inizialmente attraverso un backend esistente.

Rinvierei parser generale ambiguo, Python completo, solver arbitrari nelle
azioni e native plugin non controllati durante la speculazione. Sono linee
di ricerca possibili, ma renderebbero difficile capire quale modifica ha
prodotto il beneficio.

La proprietà da dimostrare per prima sarebbe: **una dichiarazione o una
patch accettata aggiorna in modo riproducibile il linguaggio disponibile,
e un tentativo rifiutato non ne altera lo stato**. Su questa base si possono
misurare le ulteriori garanzie senza attribuire a ZZ capacità non ancora
implementate.
