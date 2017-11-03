/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "SetupProcess.hxx"

#include <sys/signal.h>
#include <sys/prctl.h>
#include <pthread.h>

void
SetupProcess()
{
	signal(SIGPIPE, SIG_IGN);

	/* timer slack 100ms - we don't care for timer correctness */
	prctl(PR_SET_TIMERSLACK, 100000000, 0, 0, 0);

	/* reduce glibc's thread cancellation overhead */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
}

static void
UnignoreSignals()
{
	/* restore all signals which were set to SIG_IGN */
	static constexpr int signals[] = {
		SIGPIPE,
	};

	for (auto i : signals)
		signal(i, SIG_DFL);
}

static void
UnblockSignals()
{
	sigset_t mask;
	sigfillset(&mask);
	sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

void
PostFork()
{
	UnignoreSignals();
	UnblockSignals();
}
