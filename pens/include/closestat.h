/*
 * status passed to device's finish routine:
 *
 * status of CLOSE_NORMAL means normal finish,
 *
 * status of CLOSE_ERROR means fatal error condition is causing the exit,
 *
 * status of CLOSE_INTERRUPT means we just got "INTERRUPTed" (the user
 * 	hit control C, for example), and
 *
 * status of CLOSE_NOTHING means that they provided no input
 *	(dev.reset was never called, just dev.open).
 *
 * Following each of these will be a call with a status of CLOSE_DONE.
 * The device is guaranteed never to be called again after this call.
 * But that doesn't mean YOU should exit! You should return and let
 * dovplot and frontend do the exiting!!!
 *
 * Status of CLOSE_PAUSE gives us a chance to beep or do whatever should be
 * done during the pause generated by setting endpause=YES (remember that
 * the user can do this too). The sequence of calls to dev.close when closing
 * normally and with endpause=YES is PAUSE, NORMAL, DONE.
 * Many devices will simply ignore the case CLOSE_PAUSE. It is NOT the
 * responsibility of dev.close to wait for the user to do something before
 * returning! That responsibility is in dev.interact's domain.
 *
 * CLOSE_FLUSH means that the output stream to the plot device should be
 * flushed for one reason or another. Most devices should support this,
 * ESPECIALLY devices where it is possible for the user to look at the
 * output as it is produced!
 */

#define CLOSE_DONE	-1
#define CLOSE_NOTHING	 0
#define CLOSE_ERROR 	 1
#define CLOSE_INTERRUPT	 2
#define CLOSE_NORMAL 	 3
#define CLOSE_PAUSE 	 4
#define CLOSE_FLUSH	 5