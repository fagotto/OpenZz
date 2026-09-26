# ZZ prima del preprocessore C: decisioni per una futura implementazione

26 settembre 2026. Documento progettuale; nessun frontend C implementato qui.

## Decisione

La pipeline richiesta è:

```text
C esteso + definizioni sintattiche
  → ZZ
  → C con direttive e riferimenti alle macro ancora presenti
  → preprocessore C del compilatore scelto
  → analisi C, controlli semantici e compilazione
```

ZZ deve poter usare e conservare i **nomi** delle macro come parte dei costrutti
estesi, senza sostituirsi al preprocessore. Warning ed errori dovuti al risultato
delle espansioni restano responsabilità del compilatore. Non proveremo a garantire
che una macro sia valida in ogni contesto nel quale una trasformazione la usa.

Questo comprende macro definite nel sorgente, negli header e tramite opzioni
come `-D`. Un nome conservato nell'output può essere risolto successivamente,
anche quando ZZ non ne conosce la definizione. Gli argomenti di compilazione e
la scelta degli header rimangono quelli della build C.

## Nomi disponibili e valori disponibili sono concetti diversi

Esempio illustrativo, non sintassi implementata:

```c
#define CAPACITY 32

zz array int values[CAPACITY];
```

ZZ potrebbe trasformare la nuova dichiarazione in:

```c
#define CAPACITY 32
int values[CAPACITY];
```

Il valore di `CAPACITY` non serve a effettuare questa trasformazione. Può essere
conservato come riferimento opaco anche se la definizione arriva da un header.

Diverso sarebbe chiedere a ZZ di creare **CAPACITY produzioni grammaticali**:
prima del preprocessing non ne conosce necessariamente il valore. Il contratto
iniziale esclude l'uso implicito di valori di macro per decidere la grammatica.
Eventuali parametri del traduttore ZZ andranno forniti esplicitamente, separati
concettualmente dalle macro C. Anche vedere un `#define` non implica saperne
calcolare il significato, che può dipendere da altre macro e dal punto d'uso.

## Conseguenza: non pretendere un AST C completo prima delle macro

Una macro può introdurre un nome di tipo, ma anche virgole, parentesi graffe,
operatori o intere dichiarazioni. Il testo non espanso può quindi non essere
riconoscibile tramite la sola grammatica ordinaria del C.

Per un primo frontend raccomando un **trasformatore con regioni marcate**:

- riconoscere con precisione commenti, stringhe, caratteri e direttive;
- distinguere esplicitamente i costrutti estesi, inizialmente con un marcatore;
- interpretare solo le strutture necessarie alla trasformazione;
- conservare gli altri token e il testo C, senza imporre una validazione completa;
- lasciare al compilatore l'analisi dell'output dopo l'espansione.

Non basta una sostituzione tramite espressioni regolari: una parola in un commento,
una stringa o il corpo di una macro non deve attivare accidentalmente un'estensione.
Gli argomenti opachi vanno conservati senza riordinare operatori né alterare il
numero delle valutazioni. I confini delle regioni estese devono essere leggibili
prima del preprocessing; una macro non può fornire a posteriori un delimitatore
che ZZ avrebbe già dovuto riconoscere.

Un parser C contestuale completo potrebbe essere un componente successivo per
regioni che soddisfano questi requisiti. Non è una condizione per dimostrare
l'estensibilità della sintassi nella pipeline proposta.

## Direttive condizionali: politica iniziale proposta

ZZ incontra entrambi i rami di `#if`, prima che il preprocessore selezioni quello
attivo. Non conosce necessariamente le condizioni, né deve replicare le regole
sugli interi del preprocessore, `defined`, macro annidate e header.

Per la prima implementazione:

1. conservare direttive, ordine, continuazioni e contenuto C dei rami;
2. tradurre i costrutti estesi espliciti in ciascun ramo, senza eseguire il loro
   codice C e senza decidere se il ramo sarà attivo;
3. richiedere definizioni/import di sintassi ZZ in una sezione incondizionata,
   esterna ai rami `#if`; rifiutare quelli condizionali con una diagnostica ZZ;
4. evitare che un'estensione presente in un ramo modifichi il linguaggio usato
   per leggere l'altro ramo o il seguito del file.

Questa restrizione riguarda il momento in cui la grammatica cambia, non la
validità delle macro C. È un contratto iniziale, non una necessità teorica.
Un'evoluzione possibile è uno scope grammaticale separato per ciascun ramo,
con regole esplicite su quali definizioni possano uscirne.

Un costrutto ZZ malformato in un ramo poi escluso può ancora produrre un errore
ZZ: il traduttore opera prima della scelta del preprocessore. Va documentato.

## Macro che producono la nuova sintassi

Una macro che espande in una frase del linguaggio esteso arriva troppo tardi:
ZZ ha già terminato il proprio passaggio. Nella versione iniziale il suo risultato
deve essere C standard, oppure la macro deve comparire come argomento opaco di
un costrutto già riconoscibile da ZZ.

Analogamente, non trasformerei automaticamente nuove sintassi dentro le replacement
list dei `#define`: conserverei la direttiva come regione opaca. Eventuali template
ZZ capaci di generare definizioni di macro richiedono una forma esplicita e test
sulle continuazioni, sullo stringizing `#` e sul token pasting `##`.

Gli header estesi dovranno passare anch'essi per ZZ tramite regole di build
esplicite, producendo header C standard e un percorso di inclusione coerente.
Una normale `#include` letta dopo il passaggio ZZ non esegue automaticamente ZZ
sull'header incluso.

## Errori e responsabilità

| Livello | Responsabilità |
|---|---|
| ZZ | Definizioni delle estensioni, ambiguità e conflitti, struttura dei costrutti marcati, trasformazioni, limiti di risorse. |
| Preprocessore | Espansione delle macro, inclusioni, selezione condizionale e relative diagnostiche. |
| Compilatore C | Sintassi e semantica del C risultante, tipi, warning e generazione del programma. |
| Programmatore | Scelta del contesto in cui usare macro e costrutti; correttezza applicativa. |

L'errore a valle è accettabile, come richiesto. Il traduttore deve però evitare
di introdurre accidentalmente errori, doppie valutazioni o collisioni di nomi.
Serviranno una politica sui temporanei, posizioni sorgente e mappatura delle
espansioni ZZ; l'uso controllato di `#line` può migliorare le diagnostiche C.
L'output verrà pubblicato soltanto dopo una traduzione ZZ riuscita.

## Criteri per il prototipo C successivo

- Sorgenti C senza estensioni conservati, incluse direttive multilinea.
- Macro oggetto e funzione mantenute come argomenti delle estensioni.
- Macro passate con `-D` e definite in header, senza risoluzione anticipata.
- Entrambi i rami `#if` conservati e compilati in due configurazioni di prova.
- Nessuna trasformazione accidentale dentro commenti, stringhe e `#define`.
- Import sintattico condizionale rifiutato secondo il contratto iniziale.
- Macro non valida dopo la trasformazione: errore del compilatore propagato,
  senza tentativi di correggerne arbitrariamente il significato.
- Confronto del comportamento del C generato con una traduzione manuale.

## Riferimenti

- [GCC: opzioni del preprocessore](https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html).
- [GCC: condizioni `#if`](https://gcc.gnu.org/onlinedocs/cpp/If.html).

Il passo immediato resta il prototipo Python ridotto: permette di verificare
moduli di sintassi, composizione delle azioni ed emissione di frammenti Python prima
di affrontare l'interazione fra due fasi di trasformazione propria del C.

## Aggiornamento: composizione nativa delle estensioni

La verifica delle azioni del motore storico e il frontend Python in C hanno
confermato che un'estensione può richiamare altre estensioni nelle proprie azioni.
Per il futuro frontend C questo diventa un requisito: una regola utente può
esprimere la trasformazione con altri costrutti C estesi, fino alle produzioni
che generano C ordinario, senza una callback C specifica per ciascuna regola.

L'host sarà un programma C collegato a libozz, con grammatica di base e
metagrammatica per caricare file `.zz` e definizioni inline. Le primitive generiche
potranno costruire frammenti o nodi; non saranno l'unico vocabolario disponibile
agli autori delle estensioni. Le azioni native sono codice fidato eseguito durante
la traduzione, mentre il C ospite non viene eseguito dal traduttore.

La grammatica delle azioni è quella attiva al loro utilizzo, anche dopo una
ridefinizione: scope, importazioni e conflitti vanno documentati e testati. Non
si presume un significato fissato alla dichiarazione né il rollback del profilo
`--checked`. L'igiene dei temporanei e la singola valutazione restano requisiti
separati; la composizione nativa non li garantisce automaticamente.

Restano valide tutte le decisioni precedenti su ZZ prima del preprocessore,
regioni riconoscibili, macro opache, rami condizionali, header e diagnostiche.
La composizione non rende disponibili i valori delle macro prima del CPP e non
risolve la grammatica del C non ancora preprocessato. Le regioni marcate sono
una scelta iniziale di riconoscimento, non un limite alla composizione delle azioni.

Ai criteri di prova del futuro frontend C aggiungere:

- due estensioni concatenate, la seconda definita usando la prima, senza nuove
  callback C specifiche;
- definizione inline e caricamento da file, con verifica dell'ordine;
- macro conservata attraverso entrambe le espansioni fino al CPP;
- corpo annidato inserito una volta e argomenti valutati il numero previsto;
- ridefinizioni e scope controllati con esempi espliciti.

Il frontend C → C resta progettuale: in questa fase è implementato il frontend
Python ridotto → Python standard in C, documentato in `ZZPY_PROTOTIPO.md`.
