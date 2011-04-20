/*
 * Complex_Serial.c
 *
 * Complex tricky example of serial.device usage
 *
 * Compile with SAS C 5.10  lc -b1 -cfistq -v -y -L
 *
 * Run from CLI only
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <dos/dos.h>

#ifdef LATTICE
int CXBRK(void) { return(0); }  /* Disable SAS CTRL/C handling */
int chkabort(void) { return(0); }  /* really */
#endif

#define READ_BUFFER_SIZE 32

void main(void)
{
	struct MsgPort  *SerialMP;          /* Define storage for one pointer */
	struct IOExtSer *SerialIO;         /* Define storage for one pointer */

	char SerialReadBuffer[READ_BUFFER_SIZE]; /* Reserve SIZE bytes storage */

	struct IOExtSer *SerialWriteIO = 0;
	struct MsgPort  *SerialWriteMP = 0;

	ULONG Temp;
	ULONG WaitMask;

	if (SerialMP=CreatePort(0,0) )
	{
		if (SerialIO=(struct IOExtSer *)
				CreateExtIO(SerialMP,sizeof(struct IOExtSer)) )
		{
			SerialIO->io_SerFlags=0;    /* Example of setting flags */

			if (OpenDevice(SERIALNAME,0L,(struct IORequest*)SerialIO,0) )
				printf("%s did not open\n",SERIALNAME);
			else
			{
				SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
				SerialIO->io_SerFlags      &= ~SERF_PARTY_ON;
				SerialIO->io_SerFlags      |= SERF_XDISABLED;
				SerialIO->io_Baud           = 9600;
				if (Temp=DoIO((struct IORequest*)SerialIO))
					printf("Error setting parameters - code %ld!\n",Temp);

				SerialIO->IOSer.io_Command  = CMD_WRITE;
				SerialIO->IOSer.io_Length   = -1;
				SerialIO->IOSer.io_Data     = (APTR)"Amiga.";
				SendIO((struct IORequest*)SerialIO);
				printf("CheckIO %lx\n",CheckIO((struct IORequest*)SerialIO));
				printf("The device will process the request in the background\n");
				printf("CheckIO %lx\n",CheckIO((struct IORequest*)SerialIO));
				WaitIO((struct IORequest*)SerialIO);

				SerialIO->IOSer.io_Command  = CMD_WRITE;
				SerialIO->IOSer.io_Length   = -1;
				SerialIO->IOSer.io_Data     = (APTR)"Save the whales! ";
				DoIO((struct IORequest*)SerialIO);             /* execute write */


				SerialIO->IOSer.io_Command  = CMD_WRITE;
				SerialIO->IOSer.io_Length   = -1;
				SerialIO->IOSer.io_Data     = (APTR)"Life is but a dream.";
				DoIO((struct IORequest*)SerialIO);             /* execute write */

				SerialIO->IOSer.io_Command  = CMD_WRITE;
				SerialIO->IOSer.io_Length   = -1;
				SerialIO->IOSer.io_Data     = (APTR)"Row, row, row your boat.";
				SerialIO->IOSer.io_Flags = IOF_QUICK;
				BeginIO((struct IORequest*)SerialIO);

				if (SerialIO->IOSer.io_Flags & IOF_QUICK )
				{

					/*
					 * Quick IO could not happen for some reason; the device
					 *  processed the command normally.  In this case
					 *  BeginIO() acted exactly like SendIO().
					 */

					printf("Quick IO\n");
				}
				else
				{

					/* If flag is still set, IO was synchronous and is now finished.
					 * The IO request was NOT appended a reply port.  There is no
					 * need to remove or WaitIO() for the message.
					 */

					printf("Regular IO\n");
				}

				WaitIO((struct IORequest*)SerialIO);


				SerialIO->IOSer.io_Command  = CMD_UPDATE;
				SerialIO->IOSer.io_Length   = -1;
				SerialIO->IOSer.io_Data     = (APTR)"Row, row, row your boat.";
				SerialIO->IOSer.io_Flags = IOF_QUICK;
				BeginIO((struct IORequest*)SerialIO);

				if (0 == SerialIO->IOSer.io_Flags & IOF_QUICK )
				{

					/*
					 * Quick IO could not happen for some reason; the device processed
					 * the command normally.  In this case BeginIO() acted exactly
					 * like SendIO().
					 */

					printf("Regular IO\n");

					WaitIO((struct IORequest*)SerialIO);
				}
				else
				{

					/* If flag is still set, IO was synchronous and is now finished.
					 * The IO request was NOT appended a reply port.  There is no
					 * need to remove or WaitIO() for the message.
					 */

					printf("Quick IO\n");
				}


				/* Precalculate a wait mask for the CTRL-C, CTRL-F and message
				 * port signals.  When one or more signals are received,
				 * Wait() will return.  Press CTRL-C to exit the example.
				 * Press CTRL-F to wake up the example without doing anything.
				 * NOTE: A signal may show up without an associated message!
				 */

				WaitMask = SIGBREAKF_CTRL_C|
					SIGBREAKF_CTRL_F|
					1L << SerialMP->mp_SigBit;

				SerialIO->IOSer.io_Command  = CMD_READ;
				SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE;
				SerialIO->IOSer.io_Data     = (APTR)&SerialReadBuffer[0];
				SendIO((struct IORequest*)SerialIO);

				printf("Sleeping until CTRL-C, CTRL-F, or serial input\n");

				while (1)
				{
					Temp = Wait(WaitMask);
					printf("Just woke up (YAWN!)\n");

					if (SIGBREAKF_CTRL_C & Temp)
						break;

					if (CheckIO((struct IORequest*)SerialIO) ) /* If request is complete... */
					{
						WaitIO((struct IORequest*)SerialIO);   /* clean up and remove reply */

						printf("%ld bytes received\n",SerialIO->IOSer.io_Actual);
						break;
					}
				}

				AbortIO((struct IORequest*)SerialIO);  /* Ask device to abort request, if pending */
				WaitIO((struct IORequest*)SerialIO);   /* Wait for abort, then clean up */


				/*
				 * If two tasks will use the same device at the same time, it is preferred
				 * use two OpenDevice() calls and SHARED mode.  If exclusive access mode
				 * is required, then you will need to copy an existing IO request.
				 *
				 * Remember that two separate tasks will require two message ports.
				 */

				SerialWriteMP = CreatePort(0,0);
				SerialWriteIO = (struct IOExtSer *)
					CreateExtIO( SerialWriteMP,sizeof(struct IOExtSer) );

				if (SerialWriteMP && SerialWriteIO )
				{

					/* Copy over the entire old IO request, then stuff the
					 * new Message port pointer.
					 */

					CopyMem( SerialIO, SerialWriteIO, sizeof(struct IOExtSer) );
					SerialWriteIO->IOSer.io_Message.mn_ReplyPort = SerialWriteMP;

					SerialWriteIO->IOSer.io_Command  = CMD_WRITE;
					SerialWriteIO->IOSer.io_Length   = -1;
					SerialWriteIO->IOSer.io_Data     = (APTR)"A poet's food is love and fame";
					DoIO((struct IORequest*)SerialWriteIO);
				}

				if (SerialWriteMP)
					DeletePort(SerialWriteMP);

				if (SerialWriteIO)
					DeleteExtIO((struct IORequest*)SerialWriteIO);

				CloseDevice((struct IORequest*)SerialIO);
			}

			DeleteExtIO((struct IORequest*)SerialIO);
		}
		else
			printf("Unable to create IORequest\n");

		DeletePort(SerialMP);
	}
	else
		printf("Unable to create message port\n");
}

