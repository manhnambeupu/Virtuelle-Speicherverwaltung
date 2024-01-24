/********************************************************************
 * @file    syncdataexchange.h
 * @author  Franz Korf
 *          HAW-Hamburg
 *          Berliner Tor  7
 *          D-20099 Hamburg
 * @version V0.5
 * @date    Apr 2018
 * @brief   Dieses C Modul implementiert eine einfache synchrone Kommunikation
 *          zwischen vmapp (client) und dem Memory Manager (server).
 *          vmapp schreibt einen Auftrag in einen gemeinsamen Speicher. Der Server
 *          wartet auf den nächsten Auftrag, liest diesen aus, bearbeitet den Auftrag
 *          und bestätigt die erfolgreiche Bearbeitung dem Client.
 *          Da der Client auf diese Bestätigung wartet, ist die Kommunikation synchron.
 *          Der gemeinsame Speicher ist über einen Mutex geschützt. Analog zum Erzeuger
 *          Verbrauchen Problem werden zwei Semaphore zur Synchronisation zwischen Client
 *          und Server verwendet.
 *
 *          Der Server ist für die Initialiserung und Freigabe der Komponenten
 *          verantwortlich.
 *
 ******************************************************************
 */

#ifndef _SYNCDATAEXCHANGE_H
#define _SYNCDATAEXCHANGE_H
#include <stdbool.h>
#define SHMKEY_SYNC_COM            "./src/syncdataexchange.h"          //!< First paremater for shared memory generation via ftok function
#define SHMPROCID_SYNC_COM         3112                                //!< Second paremater for shared memory generation via ftok function

#define NAMED_SEM_WAKEUP_MMANAGER  "BS_A3_mmanager" //!< Semaphore to inform memory manager about new task
#define NAMED_SEM_WAKEUP_VMAPP     "BS_A3_vmapp"    //!< Semaphore to inform vmapp that task has been
/*
 * @brief Datenstruktur zur Beschreibung eines Auftrags / einer Message
 */
struct msg {
	/// @brief Der auszufuehrende Befehl
    //Mã lệnh để xác định loại nhiệm vụ (ví dụ: page fault, time interval, ACK).
	int cmd;
	/// @brief Parameter des Befehls
	int value;
	/// @brief Der g_count modelliert die aktuelle Zeit, in dem die Anzahl der
	///        Speicherzugriffe durch vmaccess gezählt wird.
	int g_count;
	/// @brief Fortlaufender Ref-Counter zur Zuordnung zwischen Befehl und Antwort.
	//Bộ đếm tham chiếu để liên kết giữa lệnh và phản hồi.
	int ref;
};

#define CMD_PAGEFAULT		1	// value gibt die einzulagernde Page mit
#define CMD_TIME_INTER_VAL   	2	// Ein Time Interval ist abgelaufen
#define CMD_ACK 		3	// value hat keine Bedeutung

/**
 * @brief  Diese Funktion erzeugt die Ressourcen, die zum synchronnen Austausch
 *         der Daten benötigt werden, von Seiten des Servers.
 *         Da die Daten vor der ersten Kommunikation vorliegen müssen, wird die
 *         Funktion von Server aufgerufen.
 */
extern void setupSyncDataExchange(void);

/**
 * @brief   Diese Funktion gibt die Ressourcen, die zum synchronnen Austausch
 *          der Daten benötigt werdeni, wieder frei.
 *          Diese Funktion wird vom Server aufgerufen.
 */
extern void destroySyncDataExchange(void);

/**
 *****************************************************************************************
 *  @brief      This function sends a message to memory manager and waits for the ACK.
 *  @param      msg Message to be send.
 *
 *  @return     void
 ****************************************************************************************/
extern void sendMsgToMmanager(struct msg msg);

/**
 *****************************************************************************************
 *  @brief      This function blocks until a message from vmapp has arrived.
 *
 *  @return     Message that has been received
 ****************************************************************************************/
extern struct msg waitForMsg(void);

/**
 *****************************************************************************************
 *  @brief      This function sends an ACK to vmapp
 *
 *  @return     Message that has been received
 ****************************************************************************************/
extern void sendAck(void);

#endif
//EOF
