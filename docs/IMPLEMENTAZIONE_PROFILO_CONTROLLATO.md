# Evoluzione ZZ: prima implementazione del profilo controllato

Data: 26 settembre 2026.

## Pubblicazione e sequenza

1. Il lavoro preesistente su robustezza e Autotools è stato salvato e pubblicato
   su `master`: commit `3b097a4`.
2. La relativa [CI](https://github.com/fagotto/OpenZz/actions/runs/36247807922)
   ha superato le quattro configurazioni originali macOS/Linux, con build
   statica e `distcheck` Linux.
3. Sul ramo `feature/checked-dynamic-grammar` il commit `1911ed9` introduce
   contesti isolati, diagnostiche, snapshot e aritmetica controllata. Prima di
   procedere sono passati i 19 test storici e il nuovo test strutturale.
4. La [PR #1](https://github.com/fagotto/OpenZz/pull/1) raccoglie il nuovo sviluppo.
   Non è stata eseguita la merge.

## Cosa è implementato

La scelta è un **profilo sperimentale separato**, disponibile nella stessa
libreria e nello stesso eseguibile tramite `ozz --checked`. Il parser storico
mantiene le proprie API e la compatibilità; non è stato convertito interamente
in un motore rientrante. Il nuovo percorso non ne esegue callback o hook.

| Capacità | Implementazione concreta |
|---|---|
| Stato isolato | `zz_context`, memoria posseduta, clone profondo e distruzione esplicita. |
| Transazioni | `/patch begin [base]`, `check`, `commit`, `abort`; candidato invisibile alle interrogazioni pubbliche. |
| Atomicità chiamata | Una `zz_context_apply` fallita non conserva modifiche a grammatica, ambiente o AST, neppure commit intermedi nella stessa chiamata. |
| Simboli e scope | `/symbol`, `/slot`, `/scope push/pop`, riferimenti a identità stabili, shadowing e binding immutabili. |
| Regole dinamiche | Famiglia deterministica di istruzioni con verbo, riferimento tipizzato, separatore ed espressione; costruzione AST tramite `CheckedAdd`, `CheckedSub`, `CheckedMul`. |
| Operatori | Precedenza e associatività configurabili per `+`, `-`, `*`. |
| Moduli | Definizione, export di regole e import con versione esatta, controllo dei conflitti; moduli conservati in memoria. |
| Esecuzione e C | Interprete AST e backend C11 autonomo; stessa politica di overflow i64 e output numerico differito. |
| Test della grammatica | Esempi positivi e negativi; durante `patch check` vengono rieseguiti tutti quelli registrati nella patch. |
| Prefissi e checkpoint | API di prova senza effetti, contesti clonabili, elenco e descrizione dei simboli visibili. |
| Politiche | Profilo lessicale ASCII, layout per righe, overflow esplicito e budget recuperabili. |

I nomi sorgente non diventano nomi C: simboli e temporanei sono emessi mediante
indici stabili. Questo impedisce collisioni con i nomi del backend nel profilo
implementato; non equivale a un sistema generale di macro igieniche.

## Esempio verificabile

`examples/checked/increment.zz` costruisce la nuova istruzione:

```text
/rule increment : Stmt -> "bump" int_ref^x "by" expr<i64>^n
  => Assign(x, CheckedAdd(Load(x), n))
```

Dopo commit della grammatica, il programma calcola `2 + 3 * 4`, incrementa il
risultato di 5 e stampa `19` e `15`. Il test CLI esegue sia l'interprete sia
il C generato e confronta gli output. Il secondo esempio dimostra i moduli.

## Verifiche

- Suite completa: **23/23 test passati** su macOS ARM64 locale e sulle build
  condivise della CI. Comprende regressioni storiche, contesti, grammatica,
  CLI/backend C e stress deterministico.
- Configurazione statica: **22 passati, 1 skip** esplicito del modulo dinamico.
- Nuovo profilo compilato separatamente in C11 con `-Wall -Wextra -Wpedantic -Werror`.
- AddressSanitizer e UndefinedBehaviorSanitizer sui test del nuovo profilo.
- Stress riproducibile: 3.000 input troncati/alterati, rollback dei fallimenti;
  100.000 coppie numeriche, confrontando addizione, sottrazione e moltiplicazione
  con gli intrinseci di controllo overflow del compilatore.
- Test di scope, isolamento dei contesti, revisione obsoleta, commit senza check,
  check invalidato da mutazioni, conflitti, import di versione errata, prefissi,
  limiti lessicali e ricorsivi, letterali fuori intervallo e overflow runtime.
- Test dell'assenza di output su errore sintattico o overflow, anche nel C generato.
- [CI del commit di implementazione](https://github.com/fagotto/OpenZz/actions/runs/36248984371):
  **7 job passati**, sei configurazioni macOS/Linux su ARM64/x86_64 e un job
  sanitizer con leak detection. Le architetture sono confermate dai log `uname`.
- `make distcheck` passato localmente e su Linux.
- Installazione in directory temporanea e consumer esterni C11/C++11: passati.
  Il controllo C++ ha individuato un campo pubblico con nome riservato, corretto
  in `is_mutable`; la verifica dell'header è stata aggiunta al job sanitizer.

Lo stress finito non è una dimostrazione universale né una campagna di fuzzing
esaustiva. Il test sanitizer del nuovo profilo non certifica il runtime storico.

## Limiti intenzionali e lavoro successivo

È implementato il primo prototipo numerico consigliato nel report di ricerca,
con operatori e moduli aggiuntivi. **Non sono implementate tutte le ipotesi di
ricerca**: regole CFG arbitrarie, nuovi tipi, template AST generali, azioni native
con contratti di effetto, lexer Unicode/indentazione Python, parsing generalizzato,
estrazione di simboli da repository e collegamento a un decoder LLM restano aperti.

Il riconoscimento dei prefissi rianalizza una copia completa. Le direttive testuali
`/prefix`, `/checkpoint` e `/explain` non sono presenti: le rispettive funzioni di
base sono esposte mediante API C. Non sono disponibili maschere di tokenizer né
una garanzia di completabilità semantica di ogni prefisso incompleto.

I simboli iniziano a zero e ogni esecuzione ricalcola l'intero AST. L'unico tipo
è i64. Le regole sono limitate alla famiglia documentata, con rifiuto esplicito
per forme esterne al profilo. I moduli non scaricano pacchetti né caricano file.
I budget sono per collezione, non un limite globale di memoria o di tempo.

I contratti completi, i comandi di riproduzione, la durata dei riferimenti restituiti
dalle API e le condizioni di errore sono in `CHECKED_PROFILE.md`. Il prossimo passo
tecnico raccomandato è generalizzare l'IR e le produzioni mantenendo questi test,
prima di introdurre lexer complessi o ottimizzazioni per il decoding.
