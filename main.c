#include <bot.h>

int init_socket(IRCBot *bot, char *nick, char *server, char *port)
{
	bot->nick = malloc(strlen(nick));
	bot->server = malloc(strlen(server));
	bot->port = malloc(strlen(port));
	strcpy(bot->nick, nick);
	strcpy(bot->server, server);
	strcpy(bot->port, port);
	bot->num_chans = 0;
	memset( &bot->hints, 0, sizeof bot->hints);
	bot->hints.ai_family = AF_UNSPEC;
	bot->hints.ai_socktype = SOCK_STREAM;
	switch(getaddrinfo( bot->server, bot->port, &bot->hints, &bot->res))
	{
		case -2:
			fprintf(stderr, "Error: Name or Service not known\n");
			return -2;
		case -3:
			fprintf(stderr, "Error: Temporary failure in name resolution\n");
			return -3;
		case -4:
			fprintf(stderr, "Error: Failure in name resolution\n");
			return -4;
		case -11:
			fprintf(stderr, "System returned error %i\n", errno);
			return errno;
	}

	// Socket!
	bot->sock = socket(bot->res->ai_family, bot->res->ai_socktype, bot->res->ai_protocol);
	printf("Attempting to connect to %s:%s\n", bot->server, bot->port);
	if(connect(bot->sock, bot->res->ai_addr, bot->res->ai_addrlen) != -1)  {
		// It worked
		printf("Connected!\n");
	} else {
		// It Failed
		fprintf(stderr, "Error: %i\n", errno);
		return errno;
	}
	return 0;
}

int init_connection(IRCBot *bot)
{
	raw(bot, "USER %s 0 0 :%s", bot->nick, bot->nick);
	nick(bot, bot->nick);
	return 1;
}

int match(const char *pattern, char *text, regmatch_t *pmatch, int size)
{
	int rc = -1;
	regex_t regexp;
	if (0 != (rc = regcomp(&regexp, pattern, REG_EXTENDED))) {
		fprintf(stderr, "regcomp() failed, returning nonzero (%d)\n", rc);
	} else {
		if (0 != (rc = regexec(&regexp, text, size, pmatch, 0))) {
			if ( rc == 1 )
				rc = -1;
		} else {
			rc = 1;
		}
	}
	regfree(&regexp);
	return rc;
}

int raw(IRCBot *bot, char *format, ...)
{
	char sendbuf[BUFFER_LEN];
	va_list args;
	va_start(args, format);
	memset(&sendbuf, 0, strlen(sendbuf));
	
	vsnprintf(sendbuf, BUFFER_LEN-1, format, args); //buf contains the formatted string
	va_end(args);
	strcpy( (sendbuf+strlen(sendbuf)), "\r\n");
	
	int len = strlen(sendbuf);
	int sent = send(bot->sock, sendbuf, len, 0);
	printf("<< %s", sendbuf);
	return sent;
}

int nick(IRCBot *bot, char *nick)
{
	bot->nick = nick;
	return raw(bot, "NICK %s", nick);
}

int join(IRCBot *bot, char *channel)
{
	int sent;
	int i;
	int cmp;
	int newIndex = 0;
	char *chanMemory;

	if ( bot->num_chans < MAX_CHANS ) {
		sent = raw(bot, "JOIN %s", channel);
	} else {
		return -1;
	}

	// Figure out where to put the new channel alphabetically.
	for (i = 0; i < bot->num_chans+1; i++) {
		if (bot->chans[i] == NULL) {
			// This is the last channel. Use it.
			newIndex = i;
			break;
		}
		if ((cmp = strcmp(bot->chans[i], channel)) == 0) {
			raw(bot, "PART %s :Memory error", channel);
			return -1;
		}
		if (cmp > 0) {
			newIndex = i;
			break;
		}
	}

	// Copy i = i-1 for all elements above the point we're adding the
	// new channel to.
	for (i = bot->num_chans+1; i > newIndex; i--) {
		bot->chans[i] = bot->chans[i-1];
	}

	// Save new channel.
	chanMemory = malloc(strlen(channel)+1);
	strcpy(chanMemory, channel);
	bot->chans[newIndex] = chanMemory;
	bot->num_chans++;

	return sent;
}

int part(IRCBot *bot, char *channel, char *format, ...)
{
	int i, l, r;
	int cmp;
	int chanIndex = MAX_CHANS+1;
	va_list ap;
	char partString[BUFFER_LEN] = {0};

	// Binary search for the channel.
	l = 0;
	r = bot->num_chans-1;
	i = (l + r)/2;
	while (chanIndex == MAX_CHANS+1) {
		if ((cmp = strcmp(bot->chans[i], channel)) < 0) {
			// The channel is in the upper range.
			l = i+1;
			i = (l + r)/2;
		}
		if (cmp == 0) {
			// Found it.
			chanIndex = i;
		}
		if (cmp > 0) {
			// The channel is in the lower range.
			r = i-1;
			i = (l + r)/2;
		}
	}

	// Index not found.
	if (chanIndex == MAX_CHANS+1) {
		return -1;
	}

	// Free memory allocated by join.
	free(bot->chans[chanIndex]);

	// Copy i = i+1 for all elements above the point we're removing the
	// channel from.
	for (i = chanIndex; i < bot->num_chans-1; i++) {
		bot->chans[i] = bot->chans[i+1];
	}

	// Part the channel and set the last element to NULL.
	bot->num_chans--;
	bot->chans[bot->num_chans] = NULL;

	va_start(ap, format);
	vsnprintf(partString, BUFFER_LEN, format, ap);
	va_end(ap);

	raw(bot, "PART %s :%s", channel, partString);

	return 0;
}

int privmsg(IRCBot *bot, char *recip, char *format, ...)
{
	char *sendbuf = malloc(BUFFER_LEN);
	int ret;
	va_list args;
	va_start(args, format);
	
	// char sendbuf[BUFFER_LEN] = {0}; perhaps?
	memset(sendbuf, 0, strlen(sendbuf));

	vsprintf(sendbuf, format, args); //buf contains the formatted string
	ret = raw(bot, "PRIVMSG %s :%s", recip, sendbuf);
	free(sendbuf);
	return ret;
}

int parse(IRCBot *bot, char *msg)
{
	char *tmp = malloc(BUFFER_LEN);
	IRCMessage *message = (IRCMessage *)malloc(sizeof (IRCMessage));
	message->sender = (IRCUser *)malloc(sizeof (IRCUser));
	int n,i;
	int sender_end, user_end, recip_end;
	regmatch_t pmatch[4];
	
	printf(">> %s\n", msg);
	
	if ( match("^PING", msg, pmatch, 1) == 1 ) {
		raw(bot, "PONG %s\r\n", &(msg[5]));
	}

	if ( match("([^: ][^! ]+[!][^@ ]+[@][^ ]+)", msg, pmatch, 1) == 1) {
		message->sender->nick = malloc(BUFFER_LEN);
		memset(message->sender->nick, 0, BUFFER_LEN);
		message->sender->user = malloc(BUFFER_LEN);
		memset(message->sender->user, 0, BUFFER_LEN);
		message->sender->host = malloc(BUFFER_LEN);
		memset(message->sender->host, 0, BUFFER_LEN);
		message->type = malloc(BUFFER_LEN);
		memset(message->type, 0, BUFFER_LEN);
		message->recip = malloc(BUFFER_LEN);
		memset(message->recip, 0, BUFFER_LEN);
		message->text = malloc(BUFFER_LEN);
		memset(message->text, 0, BUFFER_LEN);
		n = (size_t)pmatch[0].rm_so;
		strcpy(tmp, (msg+n));
		n = (size_t)pmatch[0].rm_eo;
		tmp[n-1] = '\0';
		
		n = strlen(tmp);
		for ( i = 0; i < n; i++ ) {
			switch(tmp[i]) {
				case '!':
					sender_end = i;
					break;
				case '@':
					user_end = i;
					break;
			}
		}
		strcpy(message->sender->nick, tmp);
		message->sender->nick[sender_end] = '\0';
		
		strcpy(message->sender->user, (tmp+sender_end+1));
		message->sender->user[user_end-sender_end-1] = '\0';
		
		strcpy(message->sender->host, (tmp+user_end+1));
		
		n = strlen(message->sender->nick)+strlen(message->sender->user)+strlen(message->sender->host)+4;
		strcpy(message->type, (msg+n));
		n = strlen(message->type);
		for ( i = 0; i < n; i++ ) {
			if ( message->type[i] == ' ' ) {
				recip_end = i;
				message->type[i] = '\0';
				break;
			}
		}
		
		n = strlen(message->sender->nick)+strlen(message->sender->user)+strlen(message->sender->host)+5+recip_end;
		strcpy(message->recip, (msg+n));
		n = strlen(message->recip);

		if ( *(message->recip) == ':' ) {
			message->recip+=1;
		}
		for ( i = 0; i < n; i++ ) {
			if ( message->recip[i] == ' ' ) {
				message->recip[i] = '\0';
				break;
			}
		}
		
		privmsg(bot, "#baddog", "':%s!%s@%s %s %s'", message->sender->nick, message->sender->user, message->sender->host, message->type, message->recip);
	}
	return 0;
}

int fetch_data_from_socket(IRCBot *bot, char *text)
{
	unsigned int i;
	int break_loop = 0;
	char *buf = (char*)malloc(BUFFER_LEN);
	memset(buf, 0, BUFFER_LEN);

	while ( recv(bot->sock, &(buf[strlen(buf)]), 1, 0) && (strlen(buf)<=MESSAGE_LEN) ) {
		for ( i = 0; i < MESSAGE_LEN; i++ ) {
			if(buf[i] == '\n') {
				buf[i-1] = '\0';
				break_loop = 1;
				break;
			}
		}
		if ( break_loop )
			break;
	}
	for ( i = 0; i < MESSAGE_LEN; i++) {
		if(buf[i] == '\n') {
			buf[i-1]='\0';
		}
	}
	strcpy(text, buf);
	return 0;
}

int main (int argc, char *argv[])
{
	IRCBot *bot1 = malloc(sizeof(IRCBot));
	char *text = malloc(BUFFER_LEN);
	
	init_socket(bot1, "CBot_d", "irc.eighthbit.net", "6667");
	init_connection(bot1);
	join(bot1, "#baddog");
	join(bot1, "#offtopic2");
	
	while(1) {
		fetch_data_from_socket(bot1, text);
		parse(bot1, text);
	}
	return 0;
}
