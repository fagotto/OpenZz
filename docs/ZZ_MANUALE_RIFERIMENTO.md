# OpenZz: manuale di riferimento del linguaggio nativo

**Revisione verificata:** motore storico del repository OpenZz, commit di partenza
`7e6a565`, 26 settembre 2026. Lingua: italiano. Destinatari: programmatori e agenti
che devono scrivere programmi ZZ ed estensioni grammaticali eseguibili.

Questo manuale descrive ciò che il codice registra ed esegue, non una versione
ideale del linguaggio. Copre tutte le famiglie di comandi slash registrate da
`kernel()` e `zkernel()`, incluse le forme diagnostiche e quelle inattive.
Gli esempi sono autonomi, salvo i piccoli file ausiliari mostrati accanto ad essi.

**Confine importante:** il comando `ozz file.zz` usa questo linguaggio. Il profilo
`ozz --checked` è un parser separato, documentato in [CHECKED_PROFILE.md](CHECKED_PROFILE.md).
Le direttive `/patch`, `/symbol`, `/slot`, `/rule`, `/operator`, `/module`, `/export`,
`/import syntax`, `/test`, `/numeric`, `/lexmode`, `/layout`, `/budget` e `/scope`
di quel profilo NON sono comandi del kernel storico. Non mescolare i due dialetti.
Nel frontend [ZZPy](ZZPY_PROTOTIPO.md), `import syntax`, `syntax zz`, `pylower`,
`p_expr` e `p_suite` sono regole aggiunte dall'applicazione, non primitive universali.

## Indice

1. [Avvio e convenzioni](#avvio-e-convenzioni)
2. [Modello del parser e dei valori](#modello-del-parser-e-dei-valori)
3. [Come funzionano le azioni](#come-funzionano-le-azioni)
4. [Catalogo dei comandi slash](#catalogo-dei-comandi-slash)
5. [Esempi approfonditi sulle azioni](#esempi-approfonditi-sulle-azioni)
6. [Regole pratiche per scrivere codice corretto](#regole-pratiche-per-scrivere-codice-corretto)
7. [Verifica, sorgenti e limiti](#verifica-sorgenti-e-limiti)

## Avvio e convenzioni

Salvare un esempio in `esempio.zz` ed eseguire:

```sh
ozz esempio.zz
```

Durante la verifica è utile rimuovere eventuali autocaricamenti del CLI:

```sh
env -u ZZ_INCLUDES -u ZZ_DEFAULT_INCLUDE_FILE ozz esempio.zz
```

`ZZ_DEFAULT_INCLUDE_FILE` può caricare codice prima del programma;
`ZZ_INCLUDES` cambia il prefisso di include. Il manuale non presume queste
impostazioni. La directory di lavoro conta per include e file prodotti.

- Uno statement termina con accapo o `;` nella modalità predefinita.
- Dentro una riga con più statement, usare `;`, anche dentro `{ ... }`.
- `!!` introduce un commento fino alla fine della riga; il lexer storico
  riconosce anche `//`. Usare `!!` negli esempi e nei file di grammatica.
- `...` è la continuazione di riga storica: non è un operatore di espansione.
- Le graffe delimitano liste/corpi. L'indentazione ha valore visivo, non sintattico.
- Impostare `/zlex_set_case_sensitive 1` prima di dati sensibili al caso.
  Il default storico normalizza il caso e può cambiare anche stringhe quotate.
- Le virgolette doppie formano `qstring`. Un nome non definito è normalmente
  un `ident`; un nome associato a un parametro può essere sostituito dal suo valore.
- Gli output attesi del manuale omettono gli spazi finali di `/print`.

Non usare convenzioni Python/C per dedurre quelle ZZ: l'indicizzazione di lista
parte da **1**, `as` è una ri-etichettatura, `:pass` passa il primo nonterminale,
`/return` non interrompe l'esecuzione.

## Modello del parser e dei valori

### Produzioni, terminali e nonterminali

La forma generale è `/nome -> sequenza azione`. `stat` è il nonterminale degli
statement del programma; `root` riconosce una sequenza di statement terminati.
Definire `/colore -> ...` non rende da solo `colore` uno statement: bisogna
richiamarlo da una produzione raggiungibile da `stat`.

| Notazione nella sequenza | Significato |
|---|---|
| `"rosso"` | Terminale, riconosciuto dal lexer ZZ. |
| `rosso` | Terminale scritto senza virgolette; può interferire con parametri omonimi. |
| `int^n` | Valore del nonterminale `int`, associato al parametro `n`. |
| `colore^c` | Richiamo del nonterminale definito dall'utente `colore`. |
| `int^$` | Valore senza binding nominato utile; `$` non viene installato come normale parametro. |
| Sequenza vuota | Produzione epsilon; usare con cautela per evitare ambiguità. |

Un terminale come `"=="` non deve necessariamente corrispondere a un singolo
token: il lexer storico può scomporlo in caratteri. Le ambiguità vengono segnalate,
non risolte da un ordinamento arbitrario delle alternative. Per espressioni
usare livelli grammaticali separati, come `num_e`, `$num_t`, `$num_f` nel kernel.
`/prec` non aggiunge precedenza in questa implementazione.

### Valori e categorie comuni

| Categoria | Uso |
|---|---|
| `ident`, `qstring` | Nomi e stringhe; spesso condividono una rappresentazione testuale, ma tag differenti. |
| `int`, `int64`, `float`, `double` | Valori numerici; `num_e` riconosce espressioni numeriche. |
| `list`, `list_e` | Valore lista e sintassi di espressione lista. `list` da solo non significa una lista letterale `{ ... }`. |
| `string_e` | Espressione testuale, con concatenazione `&`. |
| `$arg` | Valori accettati da print/return/assegnamento; non include automaticamente ogni nonterminale utente. |
| `lvalue` | Nome per assegnamento, anche già associato o nella forma indiretta `*nome`. |
| `param`, `gparam`, `any` | Categorie speciali del motore; preservazione/sostituzione dei parametri dipende dal contesto. |
| `NONE` | Mancanza di valore; non è un int zero né una lista vuota. |

Gli operatori numerici base sono `+ - * /` e meno unario; la divisione tra interi
è intera. Non trasferire qui gli operatori di ZZPy o Python (`%`, `and`, `or`, ecc.).
Il motore storico non garantisce aritmetica con overflow controllato.

`&` concatena testi o liste secondo le produzioni disponibili. La concatenazione
storica di testo ripassa il risultato nel lexer: può produrre un identificatore
o numero, e perdere spazi iniziali, invece di restare qstring. Non usarla come
primitiva di emissione che preserva i byte senza verificare il risultato.
Il frontend ZZPy introduce una propria `pycat` proprio per evitare tale comportamento.

Una lista è una sequenza di token/valori. Si legge con `lista . 1`, `lista . 2`,
si misura con `lista.length`. Un indice non valido può restituire NONE, che causa
errori quando riutilizzato in un'espressione. `.length` vale anche per qstring.
Sono presenti inoltre `tag_of(parametro)`, `$current_line`, `$zz$split`,
`$zz$qtoi`, `$zz$hexify`, `$zz$stringify`, `cast_to_float`: non sono statement slash
e non trasformano ZZ in un sistema di tipi statico. Per la loro grammatica esatta
consultare `/krules` o `src/kernel.c`.

## Come funzionano le azioni

### Fasi: dichiarazione, riconoscimento, interpretazione

1. Quando legge una definizione, ZZ costruisce la produzione e raccoglie il corpo
   `{ ... }` come lista di token tramite `$ablock`. Non esegue immediatamente il corpo.
2. Alcuni nomi già associati a parametri locali vengono sostituiti durante questa
   raccolta. I parametri globali hanno un trattamento differente: il loro riferimento
   può rimanere nella lista. Il corpo non è una stringa immutata del file originale.
3. Quando una produzione viene riconosciuta, i suoi nonterminali hanno valori.
   Per un'azione ZZ il motore apre uno scope di parametri e associa i nomi dopo `^`.
4. La lista del corpo diventa una nuova sorgente per `parse(root)`, con la
   **grammatica attiva in quel momento**. Può usare altre estensioni, dichiararne
   di nuove, includere file ed eseguire ulteriori azioni.
5. Il registro del risultato viene restituito come valore della produzione;
   lo scope dei parametri dell'azione viene chiuso. Le modifiche alla grammatica
   non vengono annullate automaticamente.

Questo spiega perché è possibile definire un costrutto usando un altro costrutto
appena creato. Il riferimento alla grammatica è tardivo; la cattura di un valore
locale può invece essere anticipata. Sono due aspetti distinti.

### Tre tipi di corpo e azioni speciali

| Forma | Effetto verificato |
|---|---|
| Nessuna azione | Riconosce, risultato iniziale NONE. |
| `{ statement ZZ }` | Interpreta la lista nel contesto di una nuova azione. |
| Una variabile di tipo `list` | Usa la lista già costruita come corpo dell'azione. |
| `:return valore [as tag]` | Memorizza e restituisce un valore determinato alla dichiarazione. |
| `:pass` | Restituisce il primo valore nonterminale, senza interpretare una lista. |
| `:rreturn` | Propaga il primo valore nonterminale al registro di ritorno dell'azione chiamante. |
| `:assign` | Assegnamento interno con tre argomenti nonterminali: nome, valore, tag opzionale. |

Non inventare `:list`, `:merge`, `:return parametro_formale` come scorciatoie:
i nomi di azioni interne C non sono automaticamente parole accettate dopo `:`.
`z_set_action` riconosce come azioni nominate solo `pass`, `rreturn`, `assign`;
`:return` ha una propria produzione distinta.

### Risultati, esecuzione e generazione di codice

`/return` imposta il risultato ma non fa un salto fuori dall'azione. Un helper
con corpo `{ /return x }` restituisce il valore del proprio livello; non equivale
a un helper `:rreturn`, che agisce sul livello chiamante. L'esempio dedicato
mostra la propagazione corretta.

Un blocco catturato come lista non viene eseguito solo perché si scrive il nome
nel corpo: usare `/execute blocco`, oppure definire una grammatica che consumi
quel valore. Analogamente, la stampa di `"while ..."` non esegue il ciclo Python.

Per un traduttore, le produzioni host devono restituire frammenti o nodi invece
di eseguire le operazioni del programma tradotto. Le azioni ZZ restano codice
eseguito durante la traduzione. Esecuzione di metalinguaggio e semantica del
programma generato vanno progettate separatamente.

### Due scope differenti

- **Scope dei parametri:** aperto per un'azione ZZ; contiene catture e assegnamenti.
  `/x =`, `/x :=` e `/x delta =` scelgono dove scrivere.
- **Scope della grammatica:** manipolato da `/push scope`, `/pop scope`, ecc.;
  contiene produzioni. Una regola creata dentro un'azione può sopravvivere al
  ritorno dell'azione, catturando i valori necessari.

I blocchi di `/if`, `/while`, `/for`, `/foreach` e `/execute` non devono essere
trattati come scope lessicali Python/C: nel codice corrente reinterpretano liste,
non applicano automaticamente la disciplina degli scope delle azioni di regola.

## Catalogo dei comandi slash

Le schede seguenti coprono anche le varianti di assegnamento/definizione, che
non hanno un nome di comando fisso. `/load_lib` è incluso perché inizia con slash,
ma è una produzione di espressione, non uno statement autonomo.

| Sintassi | Scheda |
|---|---|
| `/nonterminale -> sequenza [azione]` | [Definizione di produzione](#zz-rule) |
| `/(scope) nonterminale -> sequenza [azione]` | [Produzione in uno scope nominato](#zz-named-rule) |
| `/lvalue = valore [as tag]` | [Assegnazione locale](#zz-local) |
| `/lvalue := valore [as tag]` | [Assegnazione globale](#zz-global) |
| `/lvalue delta = valore [as tag]` | [Assegnazione a un livello superiore](#zz-outer) |
| `/*nome = valore` | [Nome indiretto nella lvalue](#zz-indirect) |
| `/return valore [as tag]` | [Restituire il valore di una produzione](#zz-return) |
| `/print valore [, valore ...]` | [Output](#zz-print) |
| `/error valore [, valore ...]` | [Segnalazione di errore](#zz-error) |
| `/max_error_n numero` | [Soglia degli errori](#zz-max-errors) |
| `/execute lista` | [Interpretare una lista di token](#zz-execute) |
| `/if condizione { statement } [else { statement }]` | [Condizione](#zz-if) |
| `/for nome = inizio to fine [step passo] { statement }` | [Ciclo con contatore](#zz-for) |
| `/foreach nome in lista { statement }` | [Iterazione su lista](#zz-foreach) |
| `/while condizione { statement }` | [Ciclo con test iniziale](#zz-while) |
| `/do { statement } /while condizione` | [Ciclo con test finale](#zz-do) |
| `/push scope nome` | [Attivare uno scope grammaticale](#zz-push) |
| `/pop scope` | [Disattivare lo scope superiore](#zz-pop) |
| `/delete scope nome` | [Cancellare uno scope](#zz-delete) |
| `/delpush scope nome` | [Ricreare uno scope vuoto](#zz-delpush) |
| `/when change action lista` | [Hook di sostituzione](#zz-change-hook) |
| `/when delete scope lista` | [Hook di cancellazione](#zz-delete-hook) |
| `/include "percorso.zz"` | [Includere un file](#zz-include) |
| `/include <"nome.zz">` | [Includere dalla lista di directory](#zz-include-default) |
| `/add_includedir "directory"` | [Aggiungere una directory](#zz-include-dir) |
| `/print_includedirs` | [Elencare le directory](#zz-include-dirs) |
| `/readonce identificatore` | [Guardia di inclusione](#zz-readonce) |
| `/handle = /load_lib "./modulo.so"` | [Caricare un modulo nativo](#zz-load) |
| `/subtag "figlio" "padre"` | [Copiare callback di gestione di un tag](#zz-subtag) |
| `/zlex_set_case_sensitive intero` | [Sensibilità alle maiuscole](#zz-case) |
| `/zlex_set_parse_eol intero` | [Gestione dei fine riga](#zz-eol) |
| `/zlex_set_default_real_as_double intero` | [Tipo predefinito dei reali](#zz-real) |
| `/zlex_set_default_integer_as_int64 intero` | [Tipo predefinito degli interi](#zz-integer) |
| `/prec token [numero \| right numero]` | [Precedenza: comando inattivo](#zz-prec) |
| `/version` | [Versione](#zz-version) |
| `/param` | [Parametri visibili](#zz-param) |
| `/rules [nonterminale]` | [Regole utente](#zz-rules) |
| `/krules [nonterminale]` | [Regole kernel](#zz-krules) |
| `/write rules "file.zz"` | [Esportare le regole](#zz-write-rules) |
| `/trace maschera` | [Traccia del parser](#zz-trace) |
| `/dumpnet nome` | [Rete di un nonterminale](#zz-dumpnet) |
| `/memory` | [Statistiche della memoria](#zz-memory) |
| `/report` | [Statistiche del parser](#zz-report) |
| `/lazy` | [Statistiche delle valutazioni lazy](#zz-lazy) |
| `/beep [etichetta] \| /beep reset` | [Marcatori temporali](#zz-beep) |
| `/bye` | [Terminare il processo](#zz-bye) |

<a id="zz-rule"></a>

### Definizione di produzione

**Sintassi:** `/nonterminale -> sequenza [azione]`

Installa una produzione nello scope grammaticale corrente. I terminali si scrivono preferibilmente tra virgolette; `int^n` è un nonterminale con parametro formale. Un nome senza `^` è un terminale, non un riferimento a un'altra produzione. La stessa sequenza nello stesso scope sostituisce la regola precedente. Un'azione omessa restituisce `NONE`, non il testo riconosciuto.

<!-- example:rule -->
```zz
/stat -> "saluta" ident^nome { /print "ciao", nome }
saluta anna
```

Output atteso (spazi finali omessi):

```text
ciao anna
```

<a id="zz-named-rule"></a>

### Produzione in uno scope nominato

**Sintassi:** `/(scope) nonterminale -> sequenza [azione]`

Inserisce la regola nello scope indicato, anche se non è attivo. Attivarlo con `/push scope`. Non crea uno scope di variabili.

<!-- example:named-rule -->
```zz
/(dialetto) stat -> "saluta" { /print "ciao" }
/push scope dialetto
saluta
/pop scope
```

Output atteso (spazi finali omessi):

```text
ciao
```

<a id="zz-local"></a>

### Assegnazione locale

**Sintassi:** `/lvalue = valore [as tag]`

Scrive nello scope corrente dei parametri; a livello principale questo è lo scope di base. Un'azione crea un proprio scope. `as` cambia il tag del valore: non è una conversione numerica generale. Non ri-etichettare interi come liste o puntatori. Le variabili locali visibili possono essere sostituite con il loro valore durante la costruzione di azioni/liste.

<!-- example:local -->
```zz
/x = 4
/x = x + 1
/print x
```

Output atteso (spazi finali omessi):

```text
5
```

<a id="zz-global"></a>

### Assegnazione globale

**Sintassi:** `/lvalue := valore [as tag]`

Scrive nello scope dei parametri di base e marca il parametro globale. I riferimenti globali nei corpi delle azioni possono essere risolti al loro utilizzo, anziché catturati come quelli locali. Non confondere questo scope con `/push scope`.

<!-- example:global -->
```zz
/g := 1
/stat -> "mostra" { /print g }
/g := 2
mostra
```

Output atteso (spazi finali omessi):

```text
2
```

<a id="zz-outer"></a>

### Assegnazione a un livello superiore

**Sintassi:** `/lvalue delta = valore [as tag]`

`delta` è un intero non negativo: 0 indica il livello corrente, 1 il chiamante. Se delta supera i livelli disponibili, il codice corrente scrive nel livello di base; un delta negativo causa un errore interno. Preferire delta piccoli ed espliciti.

<!-- example:outer -->
```zz
/stat -> "esporta" { /risultato 1 = 42 }
esporta
/print risultato
```

Output atteso (spazi finali omessi):

```text
42
```

<a id="zz-indirect"></a>

### Nome indiretto nella lvalue

**Sintassi:** `/*nome = valore`

È una forma di lvalue, utilizzabile dove la grammatica richiede una lvalue. La variabile contiene il nome da assegnare. Non è dereferenziazione di memoria C né commento C.

<!-- example:indirect -->
```zz
/dest = bersaglio
/*dest = 7
/print bersaglio
```

Output atteso (spazi finali omessi):

```text
7
```

<a id="zz-return"></a>

### Restituire il valore di una produzione

**Sintassi:** `/return valore [as tag]`

Imposta il registro di ritorno dell'azione. NON esce dal blocco e NON interrompe un ciclo. Una successiva `/return` può sovrascriverlo. Usarlo dentro un'azione con un chiamante che consumi il risultato.

<!-- example:return -->
```zz
/numero -> "prova" { /return 1; /print "continua"; /return 2 }
/stat -> "mostra" numero^n { /print n }
mostra prova
```

Output atteso (spazi finali omessi):

```text
continua
2
```

<a id="zz-print"></a>

### Output

**Sintassi:** `/print valore [, valore ...]`

Scrive i valori, separati da spazi, e un accapo. È presente anche uno spazio finale. Usa il canale output ZZ; non assumere che tutti i comandi diagnostici usino quel canale.

<!-- example:print -->
```zz
/print "valore", 2 + 3
```

Output atteso (spazi finali omessi):

```text
valore 5
```

<a id="zz-error"></a>

### Segnalazione di errore

**Sintassi:** `/error valore [, valore ...]`

Segnala un errore del programma ZZ; non è un'eccezione recuperabile con try/catch. Il CLI termina con esito di errore; il parser può continuare a recuperare altri statement prima di terminare.

<!-- example:error -->
```zz
/error "errore intenzionale"
```

Verifica: diagnostica contenente `errore intenzionale`.

Exit status atteso: `1` (errore intenzionale).

<a id="zz-max-errors"></a>

### Soglia degli errori

**Sintassi:** `/max_error_n numero`

Configura la soglia di errori del motore. Usare un piccolo intero positivo; non è un limite di tempo, ricorsione o memoria.

<!-- example:max-errors -->
```zz
/max_error_n 5
/print "configurato"
```

Output atteso (spazi finali omessi):

```text
configurato
```

<a id="zz-execute"></a>

### Interpretare una lista di token

**Sintassi:** `/execute lista`

Analizza la lista come `root` con la grammatica attiva. È esecuzione di ZZ, non parsing di una stringa. Una stringa con codice non è una lista. Non introduce lo scope di parametri di una nuova azione di regola; la lista stessa può avere già catturato valori alla costruzione.

<!-- example:execute -->
```zz
/programma = { /print "eseguito"; }
/execute programma
```

Output atteso (spazi finali omessi):

```text
eseguito
```

<a id="zz-if"></a>

### Condizione

**Sintassi:** `/if condizione { statement } [else { statement }]`

Esegue uno dei blocchi. Condizioni numeriche: `== != < <= > >=`; stringhe: `== !=`. `&&` e `||` sono allo stesso livello grammaticale e non hanno la gerarchia di C/Python: usare parentesi esplicite. Non promettono corto circuito: gli operandi sono ridotti prima della callback. `else` appartiene allo stesso statement.

<!-- example:if -->
```zz
/x = 2
/if (x < 3) { /print "si" } else { /print "no" }
```

Output atteso (spazi finali omessi):

```text
si
```

<a id="zz-for"></a>

### Ciclo con contatore

**Sintassi:** `/for nome = inizio to fine [step passo] { statement }`

Nel codice corrente il contatore e i limiti vengono trattati come int C; usare piccoli interi. Il test è sempre `i <= fine`: il limite è incluso, passo predefinito 1. Usare solo passi positivi: step 0 può non terminare, uno step negativo non implementa un ciclo discendente. Evitare overflow. Preferire un nome di contatore nuovo.

<!-- example:for -->
```zz
/for k = 1 to 5 step 2 { /print k }
```

Output atteso (spazi finali omessi):

```text
1
3
5
```

<a id="zz-foreach"></a>

### Iterazione su lista

**Sintassi:** `/foreach nome in lista { statement }`

Assegna ciascun elemento al contatore ed esegue il corpo. Un contatore creato dal ciclo viene rimosso al termine. Evitare nomi già usati, specialmente se il loro valore è un identificatore: sostituzioni e lvalue possono alterare il significato.

<!-- example:foreach -->
```zz
/foreach elemento in { "uno" "due" } { /print elemento }
```

Output atteso (spazi finali omessi):

```text
uno
due
```

<a id="zz-while"></a>

### Ciclo con test iniziale

**Sintassi:** `/while condizione { statement }`

La condizione viene conservata e rivalutata a ogni iterazione. Ha una grammatica numerica distinta da `/if`, non è un predicato ZZ arbitrario. **Difetto verificato della revisione documentata:** `>` e `>=` in questa grammatica sono associati alla ricostruzione `!=`. Per codice corretto usare `<` e `<=` con operandi invertiti, oppure `==`/`!=`; non copiare `>`/`>=` in un ciclo. Il corpo non introduce lo scope di un'azione.

<!-- example:while -->
```zz
/i = 0
/while (i < 3) { /print i; /i = i + 1 }
```

Output atteso (spazi finali omessi):

```text
0
1
2
```

<a id="zz-do"></a>

### Ciclo con test finale

**Sintassi:** `/do { statement } /while condizione`

Esegue almeno una volta. La coda è `/while`, con slash. Condivide la grammatica della condizione e il difetto su `>`/`>=` descritto sopra. Nessun `/break` o `/continue` nativo.

<!-- example:do -->
```zz
/i = 0
/do { /i = i + 1; /print i } /while (i < 2)
```

Output atteso (spazi finali omessi):

```text
1
2
```

<a id="zz-push"></a>

### Attivare uno scope grammaticale

**Sintassi:** `/push scope nome`

Attiva lo scope e le sue produzioni. Uno scope superiore può oscurare una regola con la stessa sequenza; non risolve automaticamente tutte le ambiguità tra sequenze differenti. Non inserire due volte uno scope già attivo.

<!-- example:push -->
```zz
/stat -> "voce" { /print "base" }
/push scope locale
/stat -> "voce" { /print "locale" }
voce
/pop scope
voce
```

Output atteso (spazi finali omessi):

```text
locale
base
```

<a id="zz-pop"></a>

### Disattivare lo scope superiore

**Sintassi:** `/pop scope`

Disattiva senza cancellare le produzioni: un push successivo le ripristina. Non elimina i parametri e non equivale a `/delete scope`. Non si può rimuovere lo scope kernel.

<!-- example:pop -->
```zz
/push scope locale
/stat -> "voce" { /print "presente" }
/pop scope
/push scope locale
voce
```

Output atteso (spazi finali omessi):

```text
presente
```

<a id="zz-delete"></a>

### Cancellare uno scope

**Sintassi:** `/delete scope nome`

Elimina le regole dello scope e chiama gli hook di cancellazione associati alle regole. Usare scope creati dall'applicazione. Per semplice sospensione usare pop.

<!-- example:delete -->
```zz
/push scope locale
/stat -> "voce" { /print "presente" }
/when delete scope { /print "rimosso" }
/delete scope locale
```

Output atteso (spazi finali omessi):

```text
rimosso
```

<a id="zz-delpush"></a>

### Ricreare uno scope vuoto

**Sintassi:** `/delpush scope nome`

Cancella lo scope omonimo e lo attiva nuovamente vuoto; gli hook di cancellazione possono essere eseguiti.

<!-- example:delpush -->
```zz
/push scope locale
/stat -> "voce" { /print "vecchio" }
/delpush scope locale
/stat -> "voce" { /print "nuovo" }
voce
```

Output atteso (spazi finali omessi):

```text
nuovo
```

<a id="zz-change-hook"></a>

### Hook di sostituzione

**Sintassi:** `/when change action lista`

Si applica all'ULTIMA regola definita, non a un nome passato al comando. L'hook della vecchia regola viene eseguito quando quella regola viene sostituita nello stesso scope. Il nuovo hook va dichiarato nuovamente se deve sopravvivere alla sostituzione.

<!-- example:change-hook -->
```zz
/stat -> "voce" { /print "vecchio" }
/when change action { /print "cambio" }
/stat -> "voce" { /print "nuovo" }
voce
```

Output atteso (spazi finali omessi):

```text
cambio
nuovo
```

<a id="zz-delete-hook"></a>

### Hook di cancellazione

**Sintassi:** `/when delete scope lista`

Si associa all'ultima regola definita; più regole nello stesso scope possono avere hook distinti. Non è un unico distruttore globale dello scope. La normale disattivazione con pop non è cancellazione.

<!-- example:delete-hook -->
```zz
/push scope locale
/stat -> "voce"
/when delete scope { /print "fine" }
/pop scope
/print "disattivo"
/delete scope locale
```

Output atteso (spazi finali omessi):

```text
disattivo
fine
```

<a id="zz-include"></a>

### Includere un file

**Sintassi:** `/include "percorso.zz"`

Interpreta il file come ZZ nel contesto corrente. Non crea automaticamente un modulo isolato. Nel CLI il prefisso dipende da `ZZ_INCLUDES` (default `./`); il percorso non è automaticamente relativo al file includente. Usare stringhe quotate e percorsi corti: le routine legacy hanno buffer fissi. La grammatica accetta anche ident e ident.ident, ma la callback assume qstring: la forma quotata è quella raccomandata.

<!-- example:include -->
```zz
/include "parte.zz"
```

File ausiliario `parte.zz`:

```zz
/print "incluso"
```

Output atteso (spazi finali omessi):

```text
incluso
```

<a id="zz-include-default"></a>

### Includere dalla lista di directory

**Sintassi:** `/include <"nome.zz">`

Cerca nelle directory aggiunte al motore, in ordine. Può aggiungere l'estensione predefinita se manca; nell'esempio il suffisso è esplicito. Non equivale a una include del preprocessore C.

<!-- example:include-default -->
```zz
/add_includedir "moduli"
/include <"parte.zz">
```

File ausiliario `moduli/parte.zz`:

```zz
/print "incluso"
```

Output atteso (spazi finali omessi):

```text
incluso
```

<a id="zz-include-dir"></a>

### Aggiungere una directory

**Sintassi:** `/add_includedir "directory"`

Aggiunge una directory alla ricerca con parentesi angolari. Non cambia la directory di lavoro. Esiste un limite fisso al numero di directory.

<!-- example:include-dir -->
```zz
/add_includedir "moduli"
/print_includedirs
```

Verifica: output contenente `moduli/`.

<a id="zz-include-dirs"></a>

### Elencare le directory

**Sintassi:** `/print_includedirs`

Mostra la lista delle directory di include predefinite; non è la stessa impostazione del prefisso `ZZ_INCLUDES`.

<!-- example:include-dirs -->
```zz
/print_includedirs
```

Verifica: output contenente `Default Include Directories:`.

<a id="zz-readonce"></a>

### Guardia di inclusione

**Sintassi:** `/readonce identificatore`

Registra una chiave globale al processo. Se la chiave era già registrata, imposta EOF sulla sorgente corrente e ne salta il resto. Usare una chiave unica per modulo; non deduce l'identità del file dal percorso.

<!-- example:readonce -->
```zz
/include "una.zz"
/include "una.zz"
```

File ausiliario `una.zz`:

```zz
/readonce modulo_unico
/print "una volta"
```

Output atteso (spazi finali omessi):

```text
una volta
```

<a id="zz-load"></a>

### Caricare un modulo nativo

**Sintassi:** `/handle = /load_lib "./modulo.so"`

È una produzione di `int`, NON uno statement `stat` autonomo: usarla come espressione. Il modulo deve esportare `void zz_ext_init(void)` e usare l'ABI di questa build. Il loader chiama l'entry point. Il risultato contiene un handle nativo etichettato int: non usarlo per aritmetica né interpretarlo come booleano portabile. Suffisso/percorso dipendono dalla piattaforma. L'esempio usa il modulo di test distribuito, copiato nel runner come `modulo.so`; non è un file già installato.

<!-- example:load -->
```zz
/handle = /load_lib "./modulo.so"
```

Verifica: output contenente `registered foo tag`.

<a id="zz-subtag"></a>

### Copiare callback di gestione di un tag

**Sintassi:** `/subtag "figlio" "padre"`

Copia solo le callback delete/param_on/param_off dal padre. NON definisce ereditarietà del parser, conversioni o compatibilità di rappresentazione. I valori devono avere una rappresentazione compatibile; il tag si può usare con `as`.

<!-- example:subtag -->
```zz
/subtag "mia_lista" "list"
/x = { 1 2 } as mia_lista
/print tag_of(x)
```

Output atteso (spazi finali omessi):

```text
mia_lista
```

<a id="zz-case"></a>

### Sensibilità alle maiuscole

**Sintassi:** `/zlex_set_case_sensitive intero`

0 usa la normalizzazione storica; un valore nonzero abilita distinzione. Impostare 1 all'inizio del file se si vuole preservare il caso. Nel CLI storico anche il testo quotato può essere normalizzato in modalità predefinita: non assumere la semantica di stringhe di Python.

<!-- example:case -->
```zz
/zlex_set_case_sensitive 1
/aa = 22
/print AA
/zlex_set_case_sensitive 0
/print AA
```

Output atteso (spazi finali omessi):

```text
AA
22
```

<a id="zz-eol"></a>

### Gestione dei fine riga

**Sintassi:** `/zlex_set_parse_eol intero`

1 rende l'accapo un separatore; 0 lo ignora e richiede `;`. Terminare esplicitamente con `;` il comando che cambia modalità; l'effetto sui token già letti dipende dalla sorgente. Non abilita una gestione dell'indentazione.

<!-- example:eol -->
```zz
/zlex_set_parse_eol 0 ;;
/x =
  22 ;
/print x ;
/zlex_set_parse_eol 1 ;
```

Output atteso (spazi finali omessi):

```text
22
```

<a id="zz-real"></a>

### Tipo predefinito dei reali

**Sintassi:** `/zlex_set_default_real_as_double intero`

Nonzero seleziona double per i successivi reali senza suffisso, 0 float. Non cambia retroattivamente valori esistenti. Impostare prima dei dati interessati.

<!-- example:real -->
```zz
/zlex_set_default_real_as_double 1
/x = 1.5
/print tag_of(x)
```

Output atteso (spazi finali omessi):

```text
double
```

<a id="zz-integer"></a>

### Tipo predefinito degli interi

**Sintassi:** `/zlex_set_default_integer_as_int64 intero`

Nonzero seleziona int64 per i successivi interi, 0 ripristina int. Le direttive che richiedono il nonterminale int possono poi non accettare un letterale diventato int64: non cambiare modalità a metà di una metagrammatica che presume int.

<!-- example:integer -->
```zz
/zlex_set_default_integer_as_int64 1
/x = 42
/print tag_of(x)
```

Output atteso (spazi finali omessi):

```text
int64
```

<a id="zz-prec"></a>

### Precedenza: comando inattivo

**Sintassi:** `/prec token [numero | right numero]`

Le tre forme sono registrate ma le callback sono commentate. Nella revisione documentata NON impostano né mostrano precedenze. Definire livelli distinti nella grammatica (espressione/termine/fattore); non basare il parsing su questo comando.

<!-- example:prec -->
```zz
/prec "+"
/prec "+" 10
/prec "*" right 20
/print "nessuna precedenza configurata"
```

Output atteso (spazi finali omessi):

```text
nessuna precedenza configurata
```

<a id="zz-version"></a>

### Versione

**Sintassi:** `/version`

Mostra la versione compilata di OpenZz; non identifica da sola il commit o le modifiche locali.

<!-- example:version -->
```zz
/version
```

Verifica: output contenente `OpenZZ version`.

<a id="zz-param"></a>

### Parametri visibili

**Sintassi:** `/param`

Stampa parametri, livelli, tag e valori. Indirizzi e dettagli del formato non sono una API stabile.

<!-- example:param -->
```zz
/x = 3
/param
```

Verifica: output contenente `x`.

<a id="zz-rules"></a>

### Regole utente

**Sintassi:** `/rules [nonterminale]`

Elenca le regole non kernel, tutte oppure del nonterminale indicato. Utile per diagnosticare ridefinizioni; non è una prova di non ambiguità.

<!-- example:rules -->
```zz
/stat -> "ciao"
/rules stat
/rules
```

Verifica: output contenente `ciao`.

<a id="zz-krules"></a>

### Regole kernel

**Sintassi:** `/krules [nonterminale]`

Elenca le produzioni INCLUDENDO quelle kernel, tutte o filtrate. Serve anche a controllare la grammatica effettivamente presente nel processo.

<!-- example:krules -->
```zz
/krules stat
/krules
```

Verifica: output contenente `/ print`.

<a id="zz-write-rules"></a>

### Esportare le regole

**Sintassi:** `/write rules "file.zz"`

Scrive le regole tramite il serializzatore diagnostico. Non presumere un checkpoint completo: stato dei parametri, librerie e risorse esterne non vengono salvati. Controllare il round trip prima di usarlo per persistenza.

<!-- example:write-rules -->
```zz
/stat -> "ciao"
/write rules "regole.zz"
```

Verifica: comando completato; il testo diagnostico dipende dalla build.

<a id="zz-trace"></a>

### Traccia del parser

**Sintassi:** `/trace maschera`

Bit pubblici: 1 riduzioni, 2 azioni ZZ, 4 scope, 8 stack LR; si combinano per somma/OR. 0 disabilita. Output molto verboso e dipendente dall’implementazione.

<!-- example:trace -->
```zz
/trace 1
/print "traccia"
/trace 0
```

Verifica: output contenente `traccia`.

<a id="zz-dumpnet"></a>

### Rete di un nonterminale

**Sintassi:** `/dumpnet nome`

Diagnostica della rete di parsing del nonterminale. Non è una definizione grammaticale e non deve essere usata come formato dati stabile.

<!-- example:dumpnet -->
```zz
/stat -> "ciao"
/dumpnet stat
```

Verifica: comando completato; il testo diagnostico dipende dalla build.

<a id="zz-memory"></a>

### Statistiche della memoria

**Sintassi:** `/memory`

Stampa contatori interni. Non è un garbage collector, un limite di memoria o una misura completa della memoria del processo.

<!-- example:memory -->
```zz
/memory
```

Verifica: output contenente `Memory usage`.

<a id="zz-report"></a>

### Statistiche del parser

**Sintassi:** `/report`

Stampa contatori accumulati dal parser. Formato e valori dipendono dall’esecuzione.

<!-- example:report -->
```zz
/report
```

Verifica: comando completato; il testo diagnostico dipende dalla build.

<a id="zz-lazy"></a>

### Statistiche delle valutazioni lazy

**Sintassi:** `/lazy`

Stampa il report interno lazy. Non attiva/disattiva la strategia di valutazione.

<!-- example:lazy -->
```zz
/lazy
```

Verifica: comando completato; il testo diagnostico dipende dalla build.

<a id="zz-beep"></a>

### Marcatori temporali

**Sintassi:** `/beep [etichetta] | /beep reset`

Stampa un marcatore temporale, file e linea; non è un suono. `reset` riavvia il riferimento interno. Usare come diagnostica, non come misura di benchmark portabile.

<!-- example:beep -->
```zz
/beep reset
/beep "fase"
/beep
```

Verifica: output contenente `TIME`.

<a id="zz-bye"></a>

### Terminare il processo

**Sintassi:** `/bye`

Chiama exit(0), anche quando ZZ è incorporato in un host C. Non significa uscire dal solo include o ritornare da una funzione. Eseguire l’esempio in un processo dedicato.

<!-- example:bye -->
```zz
/print "prima"
/bye
/print "dopo"
```

Output atteso (spazi finali omessi):

```text
prima
```

## Esempi approfonditi sulle azioni

<a id="zz-action-late"></a>

### Azioni e grammatica al momento dell’uso

Un corpo può contenere una frase non ancora definita. La grammatica attiva al momento dell’uso deve riconoscerla. Ridefinire la regola richiamata modifica le successive esecuzioni.

<!-- example:action-late -->
```zz
/stat -> "esterno" { interno }
/stat -> "interno" { /print "primo" }
esterno
/stat -> "interno" { /print "secondo" }
esterno
```

Output atteso (spazi finali omessi):

```text
primo
secondo
```

<a id="zz-action-capture"></a>

### Cattura locale e riferimento globale

Il valore locale è catturato nella definizione; il parametro globale conserva il riferimento. Questo comportamento è distinto dal binding tardivo della grammatica.

<!-- example:action-capture -->
```zz
/locale = 1
/globale := 1
/stat -> "mostra" { /print locale, globale }
/locale = 2
/globale := 2
mostra
```

Output atteso (spazi finali omessi):

```text
1 2
```

<a id="zz-action-nested"></a>

### Definire una regola dentro un’azione

La definizione interna può catturare il valore del parametro dell’azione esterna, e può essere usata immediatamente. La regola appartiene allo scope grammaticale corrente, non scompare necessariamente al termine dell’azione.

<!-- example:action-nested -->
```zz
/stat -> "crea" ident^nome {
 /stat -> nome { /print "costrutto creato" }
 nome
}
crea saluto
saluto
```

Output atteso (spazi finali omessi):

```text
costrutto creato
costrutto creato
```

<a id="zz-action-pass"></a>

### Azione :pass

Restituisce il PRIMO valore dei nonterminali della produzione. Non restituisce tutti i parametri né un AST implicito. Usare solo con almeno un nonterminale.

<!-- example:action-pass -->
```zz
/coppia -> int^a "," int^b :pass
/stat -> "prima" coppia^v { /print v }
prima 4, 9
```

Output atteso (spazi finali omessi):

```text
4
```

<a id="zz-action-constant"></a>

### Azione :return

Associa un valore alla regola, senza reinterpretare un corpo ZZ al suo uso. Il valore viene determinato nella dichiarazione. Non è equivalente a `{ /return parametro }`.

<!-- example:action-constant -->
```zz
/x = 9
/numero -> "nove" :return x
/x = 10
/stat -> "mostra" numero^n { /print n }
mostra nove
```

Output atteso (spazi finali omessi):

```text
9
```

<a id="zz-action-rreturn"></a>

### Azione :rreturn

Scrive il primo valore dei nonterminali nel registro di ritorno dell'azione chiamante. Serve per direttive che propagano un risultato; NON equivale a :pass. Questo è il meccanismo usato da `pylower` nel frontend Python.

<!-- example:action-rreturn -->
```zz
/numero -> "sette" :return 7
/stat -> "inoltra" numero^v :rreturn
/risultato -> "calcola" { inoltra sette }
/stat -> "mostra" risultato^v { /print v }
mostra calcola
```

Output atteso (spazi finali omessi):

```text
7
```

<a id="zz-action-assign"></a>

### Azione :assign

Forma speciale interna: richiede ESATTAMENTE tre valori nonterminali: identificatore, valore, tag opzionale (o NONE). Scrive un parametro locale. Non usare :assign con una normale regola a uno o due argomenti. `$argtype` fornisce il terzo valore anche quando manca `as`.

<!-- example:action-assign -->
```zz
/stat -> "memorizza" ident^n "=" int^v $argtype^t :assign
memorizza quantita = 7
/print quantita
```

Output atteso (spazi finali omessi):

```text
7
```

<a id="zz-action-list"></a>

### Azione fornita come lista

Una lista già costruita può essere associata a una regola e reinterpretata al suo riconoscimento. Le eventuali catture avvenute quando la lista è stata creata non vengono annullate.

<!-- example:action-list -->
```zz
/corpo = { /print "lista azione"; }
/stat -> "esegui" corpo
esegui
```

Output atteso (spazi finali omessi):

```text
lista azione
```

<a id="zz-action-empty"></a>

### Regola senza azione

Riconosce la frase ma non produce automaticamente un valore utile. È adatta a statement di sola sintassi; non usarla come funzione che dovrebbe restituire un int o una lista.

<!-- example:action-empty -->
```zz
/stat -> "nessuna_operazione"
nessuna_operazione
/print "ok"
```

Output atteso (spazi finali omessi):

```text
ok
```

<a id="zz-action-compose"></a>

### Composizione nativa e generazione di testo

Il livello più basso restituisce testo; un costrutto successivo usa la sintassi del livello precedente dentro l'azione. Non viene eseguito il Python generato. Questo esempio non implementa l'indentazione Python: il frontend `zzpy` documenta la variante con blocchi.

<!-- example:action-compose -->
```zz
/codice -> "scrivi" ident^id { /return "print(" & id & ")" }
/stat -> "inoltra" codice^v :rreturn
/codice -> "mostra" ident^id { inoltra scrivi id }
/stat -> "emetti" codice^v { /print v }
emetti mostra risultato
```

Output atteso (spazi finali omessi):

```text
print(risultato)
```

<a id="zz-lists"></a>

### Liste, elementi e lunghezza

Liste di token, non array Python: elementi separati da spazi, indice da UNO. `&` concatena liste. La valutazione dei nomi durante la costruzione può catturare i valori già associati.

<!-- example:lists -->
```zz
/a = { 10 20 }
/b = a & { 30 }
/print b . 1
/print b.length
```

Output atteso (spazi finali omessi):

```text
10
3
```

<a id="zz-eof"></a>

### Terminare solo la sorgente corrente

`$pretend_eof` è uno statement nativo senza slash. Termina la sorgente attuale, non il processo; un include può così restituire il controllo al chiamante.

<!-- example:eof -->
```zz
/include "fine.zz"
/print "chiamante"
```

File ausiliario `fine.zz`:

```zz
/print "interno"
$pretend_eof
/print "non eseguito"
```

Output atteso (spazi finali omessi):

```text
interno
chiamante
```

<a id="zz-loop-defect"></a>

### Prova del difetto > nei cicli

Caso di caratterizzazione, NON esempio da imitare: matematicamente `0 > 1` è falso, ma il motore di questa revisione ricostruisce `!=` ed entra nel corpo una volta. Il test documenta il difetto e segnalerà quando il comportamento cambierà.

<!-- example:loop-defect -->
```zz
/i = 0
/while (i > 1) { /print "difetto"; /i = 1 }
```

Output atteso (spazi finali omessi):

```text
difetto
```

## Regole pratiche per scrivere codice corretto

1. Dichiarare subito la modalità lessicale e usare terminali quotati nelle regole.
2. Tenere distinti nomi dei parametri ZZ, parole terminali e nomi nel linguaggio ospite.
   Un parametro già definito può sostituire un identificatore che si voleva letterale.
3. Per ogni nonterminale stabilire quale tag restituisce. Non affidarsi a un valore
   implicito di una regola senza azione.
4. Usare `{ /return cattura }` per risultati calcolati, `:return` per valori fissati
   alla dichiarazione, `:pass` solo quando si vuole il primo nonterminale.
5. Quando un helper deve propagare un risultato al chiamante, usare `:rreturn`
   come nell'esempio; non simulare un return di Python.
6. Verificare separatamente cattura dei valori, definizione di nuove regole e
   ridefinizione delle regole richiamate da altre azioni.
7. Trattare gli scope grammaticali come risorse esplicite: pop sospende, delete
   cancella. Associare gli hook subito dopo la regola cui devono appartenere.
8. Usare parentesi nelle condizioni combinate, piccoli interi nei for, passi
   positivi, e `<`/`<=` invece degli operatori difettosi nei while/do correnti.
9. Per import usare percorsi quotati, corti, con directory di lavoro nota;
   aggiungere `/readonce` se si vuole l'inclusione una sola volta.
10. Non confondere `/include`, `/execute` e `/load_lib`: leggono rispettivamente
    file ZZ, liste di token e librerie native.
11. Verificare output ed errori del CLI, non solo la presenza di un file generato.
    Nel motore storico alcuni percorsi fatali terminano direttamente il processo.
12. Non affidare grammatiche non fidate all'host: `/bye` termina il processo,
    `/load_lib` esegue codice nativo e le azioni possono modificare stato e file.
    Il motore storico non offre i contratti transazionali di `--checked`.

### Incorporare ZZ in un programma C

Le API pubbliche sono in `src/zz.h` e `src/zzbind.h`: inizializzare con `zz_init`,
registrare eventuali primitive e caricare la grammatica con `zz_parse_file` o
`zz_parse_string`. Controllare il risultato del parsing e `zz_get_error_number()`.
Il motore storico usa stato globale; non promette contesti rientranti o paralleli.

`zz_parse_string` tokenizza prima il buffer passato; non ha esattamente lo stesso
comportamento del lettore di file riga per riga, soprattutto per commenti e cambi
lessicali. Evitare di passargli un intero file con `!!` aspettandosi identico
trattamento dei fine riga: usare l'API file, oppure normalizzazione verificata.
Il driver `src/zzpy.c` mostra un'applicazione concreta, non un interprete isolato
capace di limitare tutti gli effetti delle azioni.

## Verifica, sorgenti e limiti

Il manifest [zz-reference/examples.json](zz-reference/examples.json) contiene gli
esempi esatti, gli output attesi, gli exit status e i file ausiliari.
Il runner `testsuite/zz-reference-tests.py` esegue ogni esempio in un processo e
in una directory separati, con timeout; controlla anche che gli esempi compaiano
inalterati in questo manuale. Il caso `/load_lib` usa il modulo dinamico dei test
ed è saltato esplicitamente quando la build è solo statica.

Verifica locale macOS ARM64: **59/59 esempi** superati nella build condivisa;
**58 superati e un caso dinamico saltato** nella build statica. Suite Automake
completa **26/26**; `make distcheck` superato, inclusi gli esempi dal pacchetto.

Gli output delle sonde `/memory`, `/report`, `/lazy`, `/dumpnet`, `/beep`, `/trace`
non sono formati stabili. `/dumpnet stat` può produrre byte non UTF-8 su questa
build: il test controlla il completamento, non certifica la qualità del dump.
Il caso di `/write rules` verifica anche che il file contenga la regola attesa.
Il caso del confronto difettoso nei cicli è una caratterizzazione di un problema,
non una garanzia da conservare: quando il motore verrà corretto, aggiornare test
e manuale insieme.

Fonti primarie locali:

| Sorgente | Cosa verificare |
|---|---|
| `src/kernel.c` | Statement base, espressioni, condizioni e corpi di controllo. |
| `src/zkernel.c` | Definizioni grammaticali, `$ablock`, scope, hook, direttive lessicali. |
| `src/action.c` | Esecuzione delle azioni, parametri, pass/rreturn/assign. |
| `src/zsys.c` | Collegamento delle produzioni e forme di azione accettate. |
| `src/param.c` | Scope, sostituzione e assegnamenti locali/globali. |
| `src/scope.c`, `src/rule.c` | Ridefinizione, oscuramento e hook. |
| `src/sys.c`, `src/source.c`, `src/zlex.c` | Semantica concreta di comandi, file e token. |
| `doc/src/zzdoc_tour.xml` | Esempi storici; verificare le affermazioni contro il codice corrente. |
| `testsuite/when_del_scope.zz` | Definizione e uso di un costrutto dentro una stessa azione. |

L'inventario delle registrazioni effettive è in
[zz-reference/native-rules.txt](zz-reference/native-rules.txt). È estratto dalle
produzioni C, escludendo i commenti; il test segnala se cambia senza aggiornare
l'inventario. Comandi soltanto commentati, come `/remove rules` e `/scope` nel
kernel storico, non sono funzioni disponibili.

Le verifiche non trasformano i limiti legacy in garanzie di sicurezza: restano
buffer e stack con dimensioni fisse, stato globale, assenza di igiene automatica,
possibili effetti durante la traduzione e aritmetica non interamente controllata.
Questo manuale evita i comportamenti noti problematici e li distingue dalla
semantica utilizzabile per costruire programmi corretti.
