# SERVIZIO DI MESSAGGISTICA
## 1.	SPECIFICHE
Si realizza un servizio di scambio di messaggi attraverso un server concorrente. 
Il server accetta messaggi contenenti Destinatario, Oggetto e Testo da client 
autenticati e li archivia. Ogni client oltre ad inviare messaggi ad un qualunque utente 
del sistema, può richiedere la lettura dei messaggi a lui spediti ed eventualmente la 
rimozione. 
## 2.	SCELTE DI PROGETTO 

### A.	Server
Il server è progettato per un sistema UNIX, ed è implementato come un 
processo con un thread per ogni utente connesso al sistema. Il processo del 
server è eseguito attraverso uno script (che permette di evitare di dovergli 
concedere permessi di superuser) con niceness -10, così che abbia in media 
priorità maggiore rispetto a quella di processi utente poiché esso risiede su 
un reale server Linux (2 Core, 1GB Ram) all'indirizzo 46.101.199.176:12345 
sporadicamente usato per il test di processi di rete. All'avvio del processo 
server, il main thread apre un file di log, e il file degli utenti dove sono 
salvate le credenziali associate ad ognuno (username e password) 
mappandolo in memoria per inizializzare la struttura di gestione degli utenti, 
una hashmap consultata alla connessione di ogni utente, contenente la 
password attesa. Viene inizializzata anche un'hashmap dedicata alla gestione 
delle code dei messaggi da consegnare, dove ad ogni utente è associata una 
linked list, vuota inizialmente. Entrambe le strutture verranno poi accedute 
in concorrenza, perciò gli accessi sono sincronizzati da un semaforo. La 
scelta di un semaforo unico per struttura avrebbe di fatto serializzato le 
connessioni, rendendo meno efficace il parallelismo, mentre la scelta di 
dedicare un semaforo, o una cella di semaforo, per ogni utente, sarebbe stata 
poco scalabile, a causa delle limitazioni sul numero massimo di semafori in 
UNIX, e sul numero di operazioni semaforiche richiedibili in contemporanea. 
Per questo si è scelto di gestire le strutture con un array semaforico di 
NUM_CELLE celle, e di instradare le richieste di accesso a chiavi della 
hashmap attraverso una funzione hash che generasse valori modulo 
NUM_CELLE a partire dall'username, per entrare in wait sulla cella dedicata 
al gruppo di utenti con lo stesso hash. Il server poi apre una socket in ascolto 
sulla porta 12345. Alla connessione di un nuovo client il server avvia un 
nuovo thread a lui dedicato e ne richiede l'autenticazione o la registrazione.  
Entrambe le procedure vengono gestite da una funzione di login che utilizza 
crittografia asimmetrica RSA per lo scambio dei messaggi, in modo da non 
vedere in chiaro lo scambio di password tra client e server. Poiché la chiave 
privata del server è salvata in memoria, si suppone che nessuno abbia 
accesso al file sorgente o all'eseguibile del processo server. Una volta che il 
client è registrato o autenticato, il server si mette in attesa di comandi dal 
client, che possono essere messaggi da spedire, o richieste di ricezione dei 
nuovi messaggi. Ogni messaggio ricevuto è messo in coda per la consegna 
all'utente, e salvato sul file dedicato all'utente destinatario. Un comando 
speciale, SYNC, richiede al server di inviare il log completo dei messaggi 
ricevuti dall'utente, e questo permette la gestione dello stesso account utente 
su più dispositivi client, che periodicamente risincronizzano le proprie 
caselle locali di messaggi. Il server resta in attesa di ulteriori comandi fino 
alla cessazione della connessione con il client o al timeout della socket. 
L'intero scambio di messaggi è gestito secondo crittografia a chiave 
simmetrica AES CBC 128bit con una chiave per ogni utente (la propria 
password), questo evita il leak di informazioni sensibili che potrebbero 
essere presenti nei messaggi. Il server ignora tutti i segnali a parte SIGINT 
(ignorato anche esso durante l'esecuzione di task importanti), che causa la 
chiusura del processo, dopo aver liberato le risorse dedicate al processo.


### B.	Client
Il client è progettato per un sistema WINDOWS a interfacce grafiche, ed è 
composto da un unico processo ed un unico thread, in attesa di interazioni 
utente, gestita attraverso il Message Polling. Un'applicazione client può 
gestire diversi account grazie all'autenticazione richiesta ad ogni avvio. I 
messaggi degli utenti sono salvati in locale in un file dedicato ad ogni utente, 
per permettere una gestione più snella, rispetto alla rete, dei messaggi già 
ricevuti, e richiedere al server solamente il GET dei nuovi messaggi 
indirizzati all'utente. È comunque possibile richiedere tutti i messaggi mai 
inviati all'utente, in modo da sincronizzare tutti i dispositivi collegati ad uno 
stesso account. La rimozione dei messaggi avviene sul file locale, secondo 
una filosofia di preservazione dei dati. Non viene cancellato tutto il 
messaggio, ma viene modificato solo il carattere iniziale del messaggio (+/-) 
rendendo l'operazione più efficiente, e generando un log "nascosto" e 
completo dei file mai ricevuti dall'utente. Ad ogni comando generato dal 
client, esso si autentica presso il server con le credenziali salvate, inoltra la 
richiesta e poi si disconnette, sebbene il server supporti comunque 
conversazioni composte da diversi comandi e risposte. Questo mantiene le 
socket del sistema e del server libere, e non perennemente occupate durante 
l'esecuzione dell'applicazione. 





### C.	Protocollo Messaggi
C: Client
S: Server

i.    C -> S: (Username | Psw){enc}

ii.	  S -> C: (isOk)

iii.	  If(!isOk) goto i.

iv.	  C -> S: (Cmd | [Msg]){enc}

v.	  If(Cmd = SEND) goto vii.

vi.	  S -> C: (Reply){enc}

vii.	  end


### D.	Formato Messaggi
Ogni messaggio scambiato nelle applicazioni ha una taglia massima di 
LEN_MSG ed è sempre multiplo di KEY_LEN byte. I messaggi sono scambiati 
crittografati a chiave simmetrica, il protocollo prevede che venga prima 
inviata la taglia espressa in numero di blocchi elementari AES (blocchi di 
KEY_LEN = 16 byte) del messaggio in arrivo, e venga poi spedito il messaggio 
intero. Una volta decrittato il messaggio, per essere considerato valido, deve 
contenere i campi Sender, Receiver, Object, Message separati da un byte 
RECORD_SEP, e terminare con un byte MSG_SEP, altrimenti viene scartato.
## 3.	MANUALE D'USO 

### A.	Server
Per il server è presente un Makefile nella cartella ./src, che compila i 
sorgenti, includendo automaticamente tutte le librerie necessarie (rsa.h, 
aes.h, hashmap.h, linked_list.h) e generando un eseguibile ottimizzato con -
O2.  
Per eseguire il server, lanciare lo script ./src/server.sh che esegue il 
processo in background, settando il nice a -10, senza conferire 
all'applicazione i privilegi superuser.

### B.	Client
Il client è parte di una soluzione Visual Studio: per essere compilato settare 
su Release x86 o x64. Per l'esecuzione basta lanciare l'exe generato.
