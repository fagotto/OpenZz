# ZZPy verso la sintassi completa di Python

Studio del 26 settembre 2026, sul codice `8ff7eea` e sul frontend nativo C.
**Obiettivo proposto: Python 3.14, con riferimento di verifica CPython 3.14.7.**
Non usare genericamente “Python 3”: grammatica, token e controlli cambiano fra
versioni. Al momento di implementare va fissata anche la revisione esatta della
grammatica upstream; il ramo mobile di CPython non è un contratto riproducibile.

Questo documento propone un percorso; non dichiara implementato Python completo.
Nessun parser o comportamento runtime è stato modificato per questo studio.

## Valutazione

È ragionevole proseguire con l'host C e le azioni native ZZ. Tuttavia aggiungere
produzioni al solo `base.zz` non basta. I primi interventi devono riguardare:

1. token Python con identità, valore e posizione separati;
2. gestione contestuale delle parole chiave e dei conflitti;
3. rappresentazione strutturata dei frammenti, con conservazione del testo;
4. confine fra riconoscimento ed esecuzione delle azioni;
5. limiti di memoria/stack del motore e diagnostiche recuperabili.

La composizione delle azioni già verificata resta il requisito: una regola utente
può usare altri costrutti appena definiti senza una callback C specifica. I nodi
o frammenti interni non devono diventare un vocabolario obbligatorio per l'autore.
La fattibilità dell'intera grammatica con il riconoscitore attuale NON è ancora
provata: va verificata per famiglie con test di conflitto, prima di promettere
compatibilità completa.

## Evidenze riproducibili

Il programma [zzpy-full/probe.py](zzpy-full/probe.py) ha sottoposto 41 casi scelti
a CPython 3.14.7 (`compile`, senza eseguire il programma) e al frontend ZZPy.
Risultati in [results.json](zzpy-full/results.json):

- 37 casi sono Python valido; ZZPy ne traduce 3 con AST equivalente e ne rifiuta 34;
- 4 casi sono invalidi per CPython e vengono rifiutati anche da ZZPy;
- i tre casi positivi sono espressioni base, blocchi if/while annidati e uso di
  `match`, `case`, `type` come semplici nomi **senza installare le relative sintassi**.

Non è una percentuale di copertura: il campione è deliberatamente concentrato
sulle funzionalità mancanti. Non è nemmeno una prova di equivalenza semantica
generale: AST uguali sono un controllo utile su questi casi, non un sostituto
dei test di esecuzione e degli effetti osservabili.

Tre piccole grammatiche isolate misurano il comportamento del motore ZZ:

| Prova | Risultato |
|---|---|
| [assignment-conflict.zz](zzpy-full/parser-probes/assignment-conflict.zz) | `go x = y` è ambiguo fra ridurre `target` e `atom`, quando esiste anche `==`. |
| [assignment-tokens.zz](zzpy-full/parser-probes/assignment-tokens.zz) | Distinguendo i token ASSIGN/EQ, i due casi provati vengono riconosciuti. |
| [soft-keyword.zz](zzpy-full/parser-probes/soft-keyword.zz) | `word match` viene rifiutato come uso di ident dopo l'introduzione dell'alternativa con terminale `match`; la frase completa del nuovo costrutto funziona. |

I risultati sono in [parser-results.json](zzpy-full/parser-results.json),
rigenerabili con [parser-probes.py](zzpy-full/parser-probes.py).
La seconda prova risolve quel conflitto specifico, non dimostra che basti
cambiare i token per riconoscere tutto Python.

Comandi dalla radice del repository, con i percorsi locali di build:

```sh
python3.14 docs/zzpy-full/probe.py --zzpy ../build-portable/src/zzpy --output /tmp/zzpy-coverage.json
python3.14 docs/zzpy-full/parser-probes.py --zz ../build-portable/src/ozz --output /tmp/zz-parser-probes.json
```

## 1. La grammatica ufficiale non è direttamente una grammatica ZZ

La grammatica Python pubblicata usa la notazione PEG di CPython, con alternative
ordinate, lookahead, cut e regole dedicate alle diagnostiche. Copiare le produzioni
sostituendo la punteggiatura non conserva necessariamente il linguaggio riconosciuto.
La documentazione stessa distingue il primo passaggio dalle regole `invalid_`.
Fonti: [grammatica 3.14](https://docs.python.org/3.14/reference/grammar.html),
[PEP 617](https://peps.python.org/pep-0617/).

Nel motore locale `src/parse.c`, `lr_loop` calcola candidati shift/reduce e
segnala `Ambiguous syntax` se ne resta più di uno. Non è la scelta ordinata di
un PEG né un GLR che conserva tutte le analisi. Non è corretto promettere che
una grammatica Python PEG funzioni invariata in questo algoritmo. `/prec` è
inattivo nel kernel storico e non risolve i conflitti.

### Soluzione proposta

- Usare la grammatica ufficiale come specifica e inventario, non come testo
  da tradurre meccanicamente senza verifiche.
- Convertire ripetizioni/opzionali in produzioni controllate, fattorizzare prefissi
  comuni e separare livelli di precedenza.
- Registrare per ogni regola Python: stato di implementazione, categoria dei
  risultati, esempi positivi/negativi e conflitti osservati.
- Se una famiglia richiede discriminazione, aggiungere prima predicati puri
  sul flusso di token e diagnostiche dei candidati; non una “priorità globale”
  che nasconda silenziosamente alternative valide.
- Non proporre da subito una riscrittura GLR/PEG del motore: prima dimostrare
  quali casi non si risolvono con token atomici, fattorizzazione e validazione
  strutturale. Se il prototipo di parole chiave contestuali fallisce, quello
  diventa un punto di decisione architetturale, non un problema da aggirare
  riservando sempre più parole.

## 2. Lexer Python persistente e API di token

`src/zzpy.c::tokens` oggi lavora su una sola riga, azzera la profondità delle
parentesi, riconosce pochi letterali e serializza nuovamente tutto per il lexer ZZ.
`translate` gestisce i blocchi per indentazione fisica e riconosce alcune isole
con prefissi testuali. Questo è sufficiente per il prototipo, non per Python.

Servono encoding del sorgente, Unicode, continuazioni esplicite/implicite, righe
logiche, tab e form-feed secondo la specifica, commenti e stringhe multilinea.
Gli identificatori richiedono anche le regole Unicode/NFKC. Le regole sono nella
[analisi lessicale Python 3.14](https://docs.python.org/3.14/reference/lexical_analysis.html).

### Interfaccia proposta, non ancora implementata

Un token dovrebbe portare almeno:

```text
kind, raw_spelling, semantic_payload,
start_byte, end_byte, start_line, start_column,
origin, lexical_mode
```

`kind` distingue NAME, NUMBER, STRING, operatori atomici, NEWLINE, INDENT,
DEDENT, ENDMARKER e i token delle stringhe interpolate. Commenti e spazi vanno
conservati come informazioni accessorie per ricostruire il sorgente.

L'API di ingresso di ZZ deve poter ricevere token dall'host C senza trasformarli
prima in testo e rileggerli con `zlex`. Il `source_list` interno dimostra che il
motore sa già leggere valori tokenizzati; manca un contratto pubblico adeguato
per posizioni, proprietà dei dati e confronto dei terminali host.

I nomi Python non devono diventare automaticamente parametri ZZ. Introdurre
valori/tag host distinti e catture opache; un `/x = ...` nella metagrammatica non
deve sostituire tutte le occorrenze del nome Python `x`. La corrispondenza dei
terminali come `"until"` andrà adattata a questi token senza costringere l'autore
a scrivere gli identificatori interni del lexer.

Per la prima implementazione, le estensioni host dovrebbero usare i token del
lessico Python e le isole native esplicite. Nuovi prefissi di letterale o nuovi
delimitatori richiedono un contratto lessicale aggiuntivo: non emergono
automaticamente dall'aggiunta di una produzione. Le parole nuove restano NAME;
nuovi operatori possono richiedere gestione della segmentazione e del longest match.

Anche `__name__`, `__init__` e ogni altro identificatore Python valido devono
passare: oggi il prefisso `__` è riservato per proteggere marcatori testuali.
Con token fuori banda questa restrizione può sparire.

### Come realizzare il lexer

Raccomando un componente C separato, confrontato con CPython della versione
scelta. Si può adattare il tokenizer C upstream, mantenendo una revisione precisa
e separando le dipendenze runtime, oppure implementare il contratto in C con
tabelle Unicode e test differenziali. Il primo percorso riduce alcune divergenze
ma non è una libreria pubblica stabile: il costo di adattamento va misurato.

Il modulo Python `tokenize` è utile come riferimento nei test, non come dipendenza
del preprocessore standalone. La sua documentazione limita le garanzie a codice
Python sintatticamente valido: non va usato come giustificazione per tokenizzare
qualsiasi estensione arbitraria senza un contratto lessicale proprio.
Fonte: [tokenize](https://docs.python.org/3.14/library/tokenize.html).

## 3. Parole chiave contestuali e collisioni con le estensioni

`match`, `case`, `_` e `type` hanno ruoli contestuali; non vanno riservati ovunque.
Il matching possiede una grammatica di pattern distinta dalle espressioni.
Fonti: [PEP 634](https://peps.python.org/pep-0634/),
[PEP 695](https://peps.python.org/pep-0695/).

La prova locale mostra che il semplice terminale ZZ `"match"` può catturare un
prefisso che prima era riconosciuto come ident. Lo stesso problema riguarda una
parola introdotta dall'utente, come `until`, che potrebbe anche essere un nome
Python legittimo in un'altra posizione.

### Soluzione proposta

Il lexer restituisce NAME e il parser discrimina il suo ruolo nel contesto.
Per i terminali contestuali servono predicati/lookahead senza azioni e una regola
esplicita di scelta/fallback. Distinguere nome da keyword non deve cambiare la
natura dei dati catturati. Non basta cercare un colon con una regex: il colon
può appartenere a lambda, slice, dizionari o stringhe interpolate.

Come criterio iniziale, le estensioni non devono sottrarre programmi Python
validi senza che la configurazione lo dichiari. Eventuali priorità esplicite sono
una scelta del modulo di sintassi, con conflitti diagnosticati e test negativi.

**Collisione già presente:** `import syntax` è anche Python standard per importare
un modulo chiamato `syntax`. Per una modalità compatibile raccomando:

- `import syntax` torna a essere un normale import Python;
- mantenere la forma estesa con percorso `import syntax "control.zz"`, che non è
  un normale import Python, oppure adottare un'introduzione metalinguistica distinta;
- caricare il file predefinito tramite `--grammar grammar.zz`;
- mantenere `syntax zz { ... }` come isola esplicita, senza riservare `syntax`
  come nome generale.

È una modifica di compatibilità da annunciare e testare. La gestione attuale di
qualsiasi riga che inizi con `import ` nel lexer C va rimossa: anche gli import
ordinari devono essere riconosciuti dalla grammatica host.

## 4. Costrutti, criticità e soluzioni

La tabella seguente è una proposta di scomposizione del lavoro, basata sul codice
locale e sull'inventario ufficiale. Non equivale a una verifica completa di ogni
famiglia.

| Famiglia | Problema concreto o prevedibile in ZZPy | Intervento proposto |
|---|---|---|
| Operatori e precedenze | `=` e `==` condividono token nel lexer ZZ; nuovi livelli possono creare riduzioni concorrenti. | Token atomici, livelli grammaticali, test di associatività e della relazione tra meno unario e potenza. |
| Assegnamenti | Target e normale espressione condividono prefissi; il controllo C attuale sul testo non basta. | Analisi strutturale, eventuale categoria intermedia comune e validazione Load/Store/Del; assegnamenti multipli, unpacking, annotati e aumentati distinti. |
| Chiamate e parametri | Keyword argument, positional-only, keyword-only, `*` e `**`, valori predefiniti. | Grammatica fattorizzata e validazione dell'ordine; preservare ordine delle valutazioni. |
| Tuple e collezioni | Virgola significativa, dict/set, unpacking e letterali adiacenti. | Nodi distinti e conservazione delle virgole, senza inferire tutto dalle sole parentesi. |
| Comprehension/generatori | Prefissi simili a liste/chiamate normali; clausole annidate e scope impliciti. | Famiglie grammaticali dedicate; emettere comprehension native, senza abbassarle prematuramente in cicli. |
| Confronti concatenati | Tradurre in booleani binari può rivalutare un operando. | Conservare una catena di confronto con operandi e operatori ordinati. |
| Booleani | Parentesi introdotte artificialmente possono cambiare chiamate a `__bool__`. | Conservare gruppi e catene; estendere i test già presenti. |
| Lambda, walrus, condizionale | Bassa precedenza e restrizioni diverse secondo il contesto. | Categorie di espressione specifiche; controlli sulle posizioni consentite. |
| Attributi e slicing | La lvalue C attuale accetta solo forme molto limitate. | Nodi attributo/sottoscrizione/slice; niente validazione con scansione di una stringa. |
| Suite semplici/composte | Stessa riga, `;`, decorators, elif/else, except/finally, else dei cicli. | Buffering basato sulla grammatica, non flush a ogni riga di indentazione zero. |
| Funzioni/classi/decoratori | Un decorator e la definizione seguente formano una stessa unità logica. | Riconoscimento della dichiarazione completa e conservazione delle annotazioni. |
| Async/yield | La forma sintattica non basta a decidere dove siano ammessi. | Contesto esplicito funzione/async/generatore, oppure validazione differita dichiarata. |
| Try/except/except* | Prefissi comuni, vincoli di combinazione e differenze di versione. | Rami distinti con test positivi/negativi; nessuna fusione approssimativa dei due tipi. |
| Pattern matching | Un nome nel pattern può essere una cattura; `_` non è sempre un nome ordinario. | Grammatica pattern e verifiche di binding, separata da `p_expr`. |
| Parametri di tipo/type alias | Parole contestuali e scope delle annotazioni. | Versionare le produzioni e preservare il codice, senza eseguire annotazioni durante la traduzione. |
| Commenti, future import, encoding | Rigenerare tutto può spostare docstring o import che devono precedere il codice. | Conservare trivia e ordine; inserire eventuali helper solo in posizioni consentite. |

Le proprietà di confronti, booleani e precedenze sono verificabili nella
[semantica delle espressioni](https://docs.python.org/3.14/reference/expressions.html).
L'organizzazione dei blocchi e delle dichiarazioni è descritta negli
[statement composti](https://docs.python.org/3.14/reference/compound_stmts.html).

## 5. F-string, t-string e modalità lessicali

Non basta trattare una f-string come una stringa ordinaria da copiare: per
riconoscerne la sintassi devono essere analizzati anche campi, espressioni,
conversioni e specifiche di formato. Possono servire modalità lessicali annidate.
La formalizzazione moderna è descritta nella [PEP 701](https://peps.python.org/pep-0701/).
Python 3.14 introduce anche t-string, con risultato diverso da una normale
stringa: non vanno riscritte come f-string. Fonte: [PEP 750](https://peps.python.org/pep-0750/).

Propongo uno stack di modalità del lexer: codice host, stringa semplice/tripla,
contenuto f/t-string, campo interpolato, formato annidato, isola ZZ. Conservare
il testo originale del campo debug `=`: ristamparne una versione normalizzata
può cambiare ciò che il programma mostra.

Le estensioni di tipo espressione potranno essere ammesse nei campi interpolati
solo se il contratto della modalità lo permette. Le direttive che cambiano
la grammatica restano inizialmente fuori da tali campi. Riconoscere integralmente
Python standard non implica ammettere qualunque direttiva ZZ in ogni posizione.

## 6. L'emissione attuale non può essere estesa mantenendo solo stringhe

`pycompound` indenta ogni riga del corpo. Con stringhe triple, questo può cambiare
il valore del programma. La prova in `results.json` confronta due AST:

```python
if True:
    s = """a
b"""
```

Il valore è `a\nb`. Aggiungere quattro spazi anche alla riga interna della
stringa lo cambia in `a\n    b`. Il caso è Python valido; oggi ZZPy lo rifiuta,
ma il problema apparirebbe appena si aggiungessero le stringhe triple mantenendo
l'emettitore attuale.

### Soluzione proposta

Adottare una struttura che conservi token e testo originale (CST), con nodi
strutturali almeno per le categorie che possono essere espanse. I frammenti
immutati possono essere riemessi dal sorgente; quelli generati usano un emettitore
consapevole di precedenze, righe logiche e contenuto dei letterali.

Non è necessario implementare subito un AST CPython completo. È necessario
sapere cosa è un'espressione, un target, un pattern e una suite, e dove lo spazio
è sintassi o dato. Copiare testo opaco può essere una strategia di emissione;
non deve essere presentato come riconoscimento completo delle parti saltate.

Le azioni ZZ continueranno a poter scrivere `pylower until ...` e usare blocchi
catturati; i valori trasportati diventeranno riferimenti a frammenti/nodi con
origine, anziché sole qstring. L'emettitore gestirà il reinserimento. I temporanei
introdotti dalle estensioni richiedono inoltre nomi freschi; la composizione
nativa non fornisce igiene automatica.

## 7. Azioni native e analisi speculativa

Se introduciamo fallback, lookahead o più alternative, NON possiamo eseguire
azioni native in ogni tentativo. Un'azione può cambiare grammatica, fare include,
scrivere file o produrre output: annullare lo stack del parser non annulla questi
effetti. I checkpoint del profilo `--checked` non risolvono il problema nel
motore storico.

Propongo un confine preciso:

1. tokenizzazione e predicati sono privi di effetti grammaticali;
2. l'analisi delle alternative usa solo operazioni strutturali controllate;
3. si sceglie una derivazione prima di eseguire la relativa azione utente;
4. la grammatica usata per riprendere il flusso host viene aggiornata a un confine
   esplicito; dentro un'azione già selezionata, una regola appena definita deve
   restare utilizzabile immediatamente dalle successive istruzioni native;
5. i risultati memorizzati per accelerare il parsing includono la revisione della
   grammatica nella chiave e vengono invalidati quando cambia.

La semantica delle azioni già installate deve restare quella documentata: quando
un'azione viene realmente eseguita, usa la grammatica attiva allora. Non si deve
congelarne accidentalmente il significato durante una compilazione preliminare.
Per il primo profilo completo, import/definizioni possono restare a livello modulo;
la loro esecuzione non deve dipendere da un `if` del programma Python ospite.

## 8. Limiti del motore e diagnostiche

Limiti osservati nei sorgenti, non valori consigliati:

| Punto | Limite attuale |
|---|---|
| Driver ZZPy | 32 KiB di input, 4096 token, profondità 32, 2 MiB per alcune risorse. |
| `src/zlex.c` | `MAX_TOKEN_LENGTH = 255`. La prova con un letterale di 300 caratteri fallisce, benché il driver preveda 400 byte. |
| `src/parse.c` | `WORKAREA_SIZE = 100`, `DOT_POOL_SIZE = 8000`, `LRSTACK_SIZE = 500`. |
| `src/rule.h` | `MAX_RULE_LENGTH = 30`. |
| `src/param.c` | `PARAM_SCOPE_STACK_SIZE = 50`. |

Aumentare le costanti non basta. Servono vettori dimensionati dinamicamente,
budget espliciti, controllo degli overflow e diagnostiche recuperabili. Vanno
misurati numero di stati/candidati, profondità delle azioni, memoria e tempo
sui corpus, inclusi input troncati e avversi. Le concatenazioni ripetute del driver
possono anche accumulare copie quadratiche: usare strutture condivise e flatten
solo durante l'emissione.

I token con posizione consentiranno di distinguere l'errore nel sorgente originale,
quello nel file di grammatica e quello prodotto da un'espansione, mostrando una
catena di origini. Oggi la fine dell'unità corrente non è una posizione sufficiente.
Una stringa lunga non deve attraversare necessariamente `zlex` per diventare un
singolo valore host: il trasporto di token opachi risolve quel passaggio, non tutti
gli altri buffer fissi dell'engine.

## 9. Riconoscere la grammatica non basta a validare un modulo

Un `return` fuori funzione può essere rappresentato da un AST ma non compilato
come modulo valido. Ci sono controlli su scope, binding, break/continue, await,
yield, annotazioni e assegnamenti che non coincidono con il mero riconoscimento
lessicale. La distinzione è esplicita nella documentazione di
[ast](https://docs.python.org/3.14/library/ast.html).

Separare quindi tre obiettivi e relative dichiarazioni di compatibilità:

- riconoscere la sintassi delle categorie Python della versione scelta;
- validare il contesto e i vincoli statici del modulo;
- preservare il comportamento del Python generato.

Nel percorso standalone C occorre un passaggio di validazione dei contesti,
oppure dichiarare quali controlli sono lasciati alla compilazione Python finale.
Una verifica facoltativa con CPython è utile, ma non sostituisce il parser ZZ né
può diventare una dipendenza nascosta del frontend C. Nei test usare sia `ast.parse`
sia `compile`, con flag/target coerenti, senza eseguire codice non necessario.

## 10. Piano di sviluppo con criteri di completamento

### Fase 0 — Verificare i punti architetturali

Prototipi limitati per token atomici, nome/keyword contestuale e azioni eseguite
una sola volta. Testare `match = 1` insieme a un match statement e `until = 1`
insieme a un costrutto utente. Mostrare anche un ramo fallito senza effetti.

**Uscita:** i conflitti minimi sono risolti con un contratto chiaro. Se richiedono
una riscrittura del riconoscitore, stimare e decidere quella strada prima di
ampliare tutta la grammatica. Il fallimento non va nascosto restringendo Python.

### Fase 1 — Token e lexer

API di sorgente tokenizzata, posizioni, Unicode/encoding, righe logiche,
indentazione e letterali; tab, nomi dunder e file grandi. Separazione delle isole
ZZ e rimozione della serializzazione intermedia fragile.

**Uscita:** confronto differenziale dei token sui casi validi e controllo mirato
dei casi invalidi; lettura di file standard senza collisioni con marcatori interni.
Il confronto con `tokenize` richiede di normalizzare le differenze fra API pubblica
e tokenizer interno e di non estenderne le garanzie a sorgenti arbitrari.

### Fase 2 — Struttura ed emissione

Frammenti/CST con origine, suite e letterali immutabili, emettitore che preserva
i valori delle stringhe. Portare gli esempi `until`/`tozero` alla nuova rappresentazione.

**Uscita:** trasformazione identità sui sorgenti supportati e composizione delle
estensioni dentro blocchi annidati; il test della stringa tripla mantiene il valore.

### Fase 3 — Grammatica principale

Espressioni complete, assegnamenti, dichiarazioni, import standard, funzioni,
classi, decorators, gestione errori, cicli e relativi rami. Eliminare l'euristica
che separa unità in base alle sole righe di livello zero.

**Uscita:** ogni famiglia ha prove positive/negative e inventario dei conflitti;
nessuna costruzione viene accettata solo perché lasciata opaca.

### Fase 4 — Contesti e sintassi più complesse

Comprehension, async/yield, pattern, parametri di tipo, f/t-string e controlli di
contesto. Distinguere `file`, `eval` e input interattivo: il primo obiettivo di
conformità è il modulo `.py`; gli altri ingressi devono essere dichiarati separatamente.

**Uscita:** nessuna famiglia della grammatica del target resta non implementata;
i limiti residui sono quantitativi/configurabili o dichiarati come incompatibilità.

### Fase 5 — Conformità e prestazioni

Usare corpus CPython della revisione fissata, test di grammatica/tokenizer, file
della libreria standard e progetti reali. Selezionare i test negativi per contesto:
un frammento estratto da una stringa di test non è automaticamente un modulo.

Per sorgenti validi: tradurre senza estensioni, ricompilare, confrontare AST e
comportamenti osservabili selezionati. Per quelli invalidi: verificare rifiuto e
posizioni, senza esigere subito messaggi identici a CPython. Aggiungere fuzzing,
misure di memoria/tempo, grandi file e profondità controllate.

**Uscita:** pubblicare una matrice riproducibile di versione, corpus, modalità,
limiti e risultati. “Sintassi completa” non può derivare solo da un alto numero
di esempi positivi.

## Decisione raccomandata

Iniziare dalla Fase 0 e dall'API tokenizzata della Fase 1. Conservare host C,
metagrammatica e composizione nativa; sostituire il trasporto testuale e il modello
di emissione troppo semplice. Non aggiungere subito tutte le produzioni a
`base.zz`: i tre conflitti/problemi verificati mostrano perché produrrebbe una
catena di eccezioni invece di una base adatta alla grammatica completa.

Le prove sono strumenti di ricerca separati dalla suite di regressione: alcuni
input devono fallire nel prototipo attuale. Non bloccano la CI in attesa delle
funzionalità ancora da implementare e non modificano il contratto esistente.
