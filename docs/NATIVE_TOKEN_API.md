# Token esterni per il parser nativo ZZ

## Obiettivo e separazione architetturale

Questa PR parte da master e riguarda esclusivamente libozz. Non dipende
né dal profilo i64 controllato né da ZZPy. È il primo incremento della
proposta per supportare linguaggi con lexer esterni: non è ancora
l'implementazione della sintassi completa di Python.

La libreria gestisce il flusso di token tipizzati, l'integrazione con il
parser nativo, la posizione di input e l'esecuzione delle azioni esistenti.
Il frontend sceglie le regole lessicali, produce i token, definisce la
grammatica e implementa tramite azioni le strutture del linguaggio ospite.
Nessun trattamento di indentazione, keyword o stringhe Python è nel core.

## API

Includere `ozz/zz_tokens.h`, inizializzare la libreria con `zz_init()` e
registrare la grammatica. Chiamare:

```c
int zz_parse_tokens(const char *name, zz_token_reader reader, void *user);
```

Il callback riceve un `struct zz_token *` inizializzato a zero. Assegna
`value.tag`, il payload appropriato in `value.val` e opzionalmente `span`.
Restituisce `ZZ_TOKEN_OK`, `ZZ_TOKEN_END` o `ZZ_TOKEN_ERROR`.
La funzione restituisce 1 se non sono stati segnalati errori durante la
chiamata, 0 altrimenti. Un flusso vuoto è valido per la grammatica root.
Il contatore globale degli errori non viene azzerato.

Il tipo è un tag ZZ registrato, non un nuovo enum specifico del frontend.
Usare `tag_ident`, `tag_int`, `tag_qstring`, `tag_eol` oppure tag applicativi
registrati tramite l'API esistente. I puntatori ai tag devono essere validi.
Gli identificatori vengono internati da ZZ per il confronto con terminali
letterali. Non avviene normalizzazione di maiuscole/minuscole o Unicode.
I payload non sono passati nuovamente al lexer nativo: una stringa lunga
non incontra il suo limite di 255 caratteri. Restano i limiti del parser.

`span` contiene offset byte semiaperti e riga/colonna a partire da 1;
zero indica una posizione sconosciuta. Le righe devono rientrare in INT_MAX,
per compatibilità con la diagnostica storica. Nome, riga, colonna e offset
sono disponibili nella diagnostica della sorgente esterna; non sono ancora
propagati come intervalli di ogni riduzione o come catene di espansione.
I token EOL hanno payload normalizzato a zero. EOF si segnala mediante
`ZZ_TOKEN_END`: i tag EOF, continuazione nativa e parametro interno non
sono ammessi come token esterni.

## Proprietà e durata

Lettura sincrona. Il reader non deve richiamare il parser, mutare la grammatica
o alterare lo stato ZZ. Le normali azioni possono invece definire regole
ed eseguire azioni native composte come prima.

Nome della sorgente e contesto del reader sono presi in prestito per la
chiamata. I payload sono presi in prestito: devono restare validi per tutta
la chiamata e finché regole o parametri ne conservano riferimenti. Non è
sicuro restituire puntatori a buffer locali del callback o riusare lo stesso
buffer per token diversi. La libreria non introduce nuovi distruttori o
trasferimenti di proprietà. Usare inizializzazione a zero per s_content,
compresa l'unione, come nell'esempio eseguibile.

Gli identificatori provenienti dal reader non subiscono la sostituzione
automatica con parametri ZZ. All'interno delle azioni native rimane attivo
il comportamento ordinario: i nomi dei parametri catturati si risolvono
normalmente. Questo separa i nomi del linguaggio ospite dai metaparametri.

Non c'è rollback: azioni precedenti a un errore possono aver modificato lo
stato. Non si aggiungono isolamento, concorrenza, sandbox o esecuzione
speculativa. Il vecchio parser è ancora globale e con limiti fissi.

## Esempio eseguibile e verifiche

`testsuite/token-input.c` è un host C completo, incluso in `make check`.
Definisce regole con callback e alimenta il parser senza serializzare testo.
Verifica:

- il nome `host` resta un identificatore anche se `/host := 42` esiste;
- una stringa da 1023 caratteri arriva integralmente all'azione;
- un'azione definisce `fresh` e la invoca subito, con una sola chiamata
  al callback finale;
- errori del reader, token senza tag e callback nullo sono respinti;
- il flusso vuoto e una nuova chiamata dopo un errore funzionano.

Validazione locale macOS: suite dinamica 20/20, statica 19 passati e
1 skip previsto per il modulo dinamico; `make distcheck` superato. Il test non dimostra la
correttezza di azioni speculative: il riconoscitore non è stato modificato
per introdurre alternative speculative.

## Incrementi successivi, ancora da implementare

1. Prove generiche di terminali contestuali e disambiguazione, indipendenti
   da Python; definizione del contratto puro dei predicati di riconoscimento.
2. Catture con gestione esplicita della durata, posizioni delle riduzioni
   e catene di origine delle espansioni.
3. Limiti configurabili e fallimenti recuperabili delle strutture interne.
4. Eventuali utilità riusabili per modalità lessicali e indentazione.
5. Adozione in ZZPy in una PR distinta: lexer Python, grammatica, controlli
   contestuali, rappresentazione strutturata ed emissione Python.

Una callback di azione eseguita dopo il riconoscimento non risolve un
conflitto che impedisce il riconoscimento stesso. La nuova API token non
risolve da sola il conflitto tra parola contestuale e identificatore né
converte il parser in PEG o GLR. Questi punti richiedono verifiche separate
prima di estendere l'intera grammatica Python.
