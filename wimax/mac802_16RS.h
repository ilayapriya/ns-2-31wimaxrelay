/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 * and makes no guarantees, expressed or implied, about its quality,
 * reliability, or any other characteristic.
 * <BR>
 * We would appreciate acknowledgement if the software is used.
 * <BR>
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 */

#ifndef MAC802_16RS_H
#define MAC802_16RS_H

#include "mac802_16.h"
#include "scheduling/wimaxctrlagent.h"
#include "scheduling/scanningstation.h"

//Subscription related codes
#define BS_NOT_CONNECTED -1 //bs_id when MN is not connected

/** Defines the state of the MAC */
enum Mac802_16State {
    MAC802_16_DISCONNECTED,
    MAC802_16_WAIT_DL_SYNCH,
    MAC802_16_WAIT_DL_SYNCH_DCD,
    MAC802_16_UL_PARAM,
    MAC802_16_RANGING,
    MAC802_16_WAIT_RNG_RSP,
    MAC802_16_REGISTER,
    MAC802_16_SCANNING,
    MAC802_16_CONNECTED
};

/** Data structure to store MAC state */
struct state_info {
    Mac802_16State state;
    int bs_id;
    double frameduration;
    int frame_number;
    int channel;
    ConnectionManager * connectionManager;
    ServiceFlowHandler * serviceFlowHandler;
    struct peerNode *peer_list;
    int nb_peer;
};

/** The sub state (while connected) */
enum rs_sub_state {
    NORMAL,           //Normal state
    SCAN_PENDING,     //Normal period but pending scanning to start/resume
    SCANNING,         //Currently scanning
    HANDOVER_PENDING, //Normal state but handover to start
    HANDOVER          //Executing handover
};

/** Data structure to store scanning information */
struct scanning_structure {
    struct mac802_16_mob_scn_rsp_frame *rsp; //response from BS
    struct sched_state_info scan_state;     //current scanning state
    struct sched_state_info normal_state;   //backup of normal state
    int iteration;                          //current iteration
    WimaxScanIntervalTimer *scn_timer_;     //timer to notify end of scanning period
    int count;                              //number of frame before switching to scanning
    rs_sub_state substate;
    WimaxNeighborEntry *nbr; //current neighbor during scanning or handover
    //arrays of rdv timers
    WimaxRdvTimer *rdv_timers[2*MAX_NBR];
    int nb_rdv_timers;
    //handoff information
    int serving_bsid;
    int handoff_timeout; //number frame to wait before executing handoff
};


//Some configuration elements to enable/disable features under development
#define BWREQ_PATCH

/** Information about a new client */
struct new_client_t {
    int cid; //primary cid of new client
    new_client_t *next;
};

class T17Element;
LIST_HEAD (t17element, T17Element);
/** Object to handle timer t17 */
class T17Element {
public:
    T17Element (Mac802_16 *mac, int index) {
        index_ = index;
        timer = new WimaxT17Timer (mac, index);
        timer->start (mac->macmib_.t17_timeout);
    }

    ~T17Element () {
        delete (timer);
    }

    int index () {
        return index_;
    }
    int paused () {
        return timer->paused();
    }
    int busy () {
        return timer->busy();
    }
    void cancel () {
        timer->stop();
    }

    // Chain element to the list
    inline void insert_entry(struct t17element *head) {
        LIST_INSERT_HEAD(head, this, link);
    }

    // Return next element in the chained list
    T17Element* next_entry(void) const {
        return link.le_next;
    }

    // Remove the entry from the list
    inline void remove_entry() {
        LIST_REMOVE(this, link);
    }
protected:

    /*
     * Pointer to next in the list
     */
    LIST_ENTRY(T17Element) link;
    //LIST_ENTRY(T17Element); //for magic draw

private:
    int index_;
    WimaxT17Timer *timer;
};

class FastRangingInfo;
LIST_HEAD (fastRangingInfo, FastRangingInfo);
/** Store information about a fast ranging request to send */
class FastRangingInfo {
public:
    FastRangingInfo (int frame, int macAddr) {
        frame_ = frame;
        macAddr_ = macAddr;
    }

    int frame () {
        return frame_;
    }
    int macAddr () {
        return macAddr_;
    }

    // Chain element to the list
    inline void insert_entry(struct fastRangingInfo *head) {
        LIST_INSERT_HEAD(head, this, link);
    }

    // Return next element in the chained list
    FastRangingInfo* next_entry(void) const {
        return link.le_next;
    }

    // Remove the entry from the list
    inline void remove_entry() {
        LIST_REMOVE(this, link);
    }
protected:

    /*
     * Pointer to next in the list
     */
    LIST_ENTRY(FastRangingInfo) link;
    //LIST_ENTRY(FastRangingInfo); //for magic draw

private:
    int frame_;
    int macAddr_;
};


/**
 * Class implementing IEEE 802_16 State machine at the RS
 */
class Mac802_16RS : public Mac802_16 {
    friend class WimaxCtrlAgent;
    friend class RSScheduler;
    friend class BwRequest;
public:

    Mac802_16RS();
    /*
     * Scan channel
     * @param req the scan request information
     */
    void link_scan (void *req);
    /**
     * Set the mac state
     * @param state The new mac state
     */
    void setMacState (Mac802_16State state);

    /**
     * Return the mac state
     * @return The new mac state
     */
    Mac802_16State getMacState ();

    /**
     * Creates a snapshot of the MAC's state and reset it
     * @return The snapshot of the MAC's state
     */
    state_info *backup_state ();

    /**
     * Restore the state of the Mac
     * @param state The state to restore
     */
    void restore_state (state_info *state);

    inline u_char get_diuc() {
        return default_diuc_;
    }

    /* initial random propagation channel
     *rpi
     */
    int  GetInitialChannel();


    /**
     * Interface with the TCL script
     * @param argc The number of parameter
     * @param argv The list of parameters
     */
    int command(int argc, const char*const* argv);

    /**
     * Process packets going out
     * @param p The packet to transmit
     */
    void sendDown(Packet *p);

    /**
     * Process packets going out
     * @param p The packet to transmit
     */
    void transmit(Packet *p);

    /**
     * Process incoming packets
     * @param p The received packet
     */
    void sendUp(Packet *p);

    /**
     *sam intpower_
     */
    //double intpower_[1024];

    /**
     * Process the packet after receiving last bit
     */
    //void receive();

    // fille the frame with power values
    void addPowerinfo(hdr_mac802_16 *wimaxHdr,double power, bool collision );

    //chk for collision
    bool IsCollision (const hdr_mac802_16 *wimaxHdr,double power_subchannel);


    /**
     * Process the packet after receiving last bit
     *@param p - the packet to be received  RPI
     */
    void receive(Packet *p);

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
    /*
     * Configure/Request configuration
     * The upper layer sends a config object with the required
     * new values for the parameters (or PARAMETER_UNKNOWN_VALUE).
     * The MAC tries to set the values and return the new setting.
     * For examples if a MAC does not support a parameter it will
     * return  PARAMETER_UNKNOWN_VALUE
     * @param config The configuration object
     */
    void link_configure (link_parameter_config_t* config);

#endif

protected:

    /**
     * Called when lost synchronization
     */
    void lost_synch ();


    /**
     * Start/Continue scanning
     */
    void resume_scanning ();

    /**
     * Pause scanning
     */
    void pause_scanning ();




    /**
     * init the timers and state
     */
    void init ();

    /**
     * Initialize default connection
     */
    void init_default_connections ();

    /**
     * Update the given timer and check if thresholds are crossed
     * @param watch the stat watch to update
     * @param value the stat value
     */
    void update_watch (StatWatch *watch, double value);

    /**
     * Update the given timer and check if thresholds are crossed
     * @param watch the stat watch to update
     * @param size the size of packet received
     */
    void update_throughput (ThroughputWatch *watch, double size);

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
    /**
     * Poll the given stat variable to check status
     * @param type The link parameter type
     */
    void poll_stat (link_parameter_type_s type);
#endif

    /**
     * Called when a timer expires
     * @param The timer ID
     */
    virtual void expire (timer_id id);

    /**
     * Start a new DL subframe
     */
    virtual void start_dlsubframe ();

    /**
     * Process cdma bandwidth request
     * @param p The request
     */
    void process_cdma_req (Packet *p);

    /**
     * Start a new UL subframe
     */
    virtual void start_ulsubframe ();

    /**
     * Finds out if the given station is currently scanning
     * @param nodeid The MS id
     */
    bool isPeerScanning (int nodeid);

    /**
     * Set the control agent
     * @param agent The control agent
     */
    void setCtrlAgent (WimaxCtrlAgent *agent);

    /** Add a new Fast Ranging allocation
     * @param time The time when to allocate data
     * @param macAddr The MN address
     */
    void addNewFastRanging (double time, int macAddr);

    /**
     * Send a scan response to the MN
     * @param rsp The response from the control
     * @cid The CID for the MN
     */
    void send_scan_response (mac802_16_mob_scn_rsp_frame *rsp, int cid);

    /**
     * Indicates if it is time to send a DCD message
     */
    bool sendDCD;

    /**
     * DL configuration change count
     */
    int dlccc_;

    /**
     * Indicates if it is time to send a UCD message
     */
    bool sendUCD;

    /**
     * UL configuration change count
     */
    int ulccc_;

private:

    bool ul;
    bool connected;
    /*
    * The state of the MAC
    */
    Mac802_16State state_;

    /**
    * Timers
    */
    WimaxT1Timer  *t1timer_;
    WimaxT2Timer  *t2timer_;
    WimaxT6Timer  *t6timer_;
    WimaxT12Timer *t12timer_;
    WimaxT21Timer *t21timer_;
    WimaxLostDLMAPTimer *lostDLMAPtimer_;
    WimaxLostULMAPTimer *lostULMAPtimer_;
    WimaxT44Timer *t44timer_;
    /**
     * Current number of registration retry
     */
    u_int32_t nb_reg_retry_;

    /**
     * Current number of scan request retry
     */
    u_int32_t nb_scan_req_;
    /**
     * The scanning information
     */
    struct scanning_structure *scan_info_;



    /**
     * Process a DL_MAP message
     * @param frame The dl_map information
     */
    void process_dl_map (mac802_16_dl_map_frame *frame);

    /**
     * Process a DCD message
     * @param frame The dcd information
     */
    void process_dcd (mac802_16_dcd_frame *frame);

    /**
     * Process a UL_MAP message
     * @param frame The ul_map information
     */
    void process_ul_map (mac802_16_ul_map_frame *frame);

    /**
     * Process a UCD message
     * @param frame The ucd information
     */
    void process_ucd (mac802_16_ucd_frame *frame);

    /**
     * Process a ranging response message
     * @param frame The ranging response frame
     */
    void process_ranging_rsp (mac802_16_rng_rsp_frame *frame);

    /**
     * Process a registration response message
     * @param frame The registration response frame
     */
    void process_reg_rsp (mac802_16_reg_rsp_frame *frame);

    /**
     * Schedule a ranging
     */
    void init_ranging ();

    /**
     * Prepare to send a registration message
     */
    void send_registration ();

    /**
     * Send a scanning message to the serving BS
     */
    void send_scan_request ();

    /**
     * Process a scanning response message
     * @param frame The scanning response frame
     */
    void process_scan_rsp (mac802_16_mob_scn_rsp_frame *frame);

    /**
     * Process a BSHO-RSP message
     * @param frame The handover response frame
     */
    void process_bsho_rsp (mac802_16_mob_bsho_rsp_frame *frame);

    /**
     * Process a BSHO-RSP message
     * @param frame The handover response frame
     */
    void process_nbr_adv (mac802_16_mob_nbr_adv_frame *frame);

    /**
     * Send a MSHO-REQ message to the BS
     */
    void send_msho_req ();

    /**
     * Check rdv point when scanning
     */
    void check_rdv ();

    /**
     * Set the scan flag to true/false
     * param flag The value for the scan flag
     */
    void setScanFlag(bool flag);

    /**
     * return scan flag
     * @return the scan flag
     */
    bool isScanRunning();


    //Begin RPI
    /**
     * The function is used to process the MAC PDU when ARQ,Fragmentation and Packing are enabled
     * @param con The connection by which it arrived
     * @param p The packet to process
     */
    void process_mac_pdu_witharqfragpack (Connection *con, Packet *p);
    //End RPI

    /**
     * Process a MAC type packet
     * @param con The connection by which it arrived
     * @param p The packet to process
     */
    void process_mac_packet (Connection *con, Packet *p);

    /**
     * Process a RNG-REQ message
     * @param frame The ranging request information
     */
    void process_ranging_req (Packet *p);

    /**
     * Process bandwidth request
     * @param p The request
     */
    void process_bw_req (Packet *p);

    /**
     * Process bandwidth request
     * @param p The request
     */
    void process_reg_req (Packet *p);

    /**
     * Process handover request
     * @param req The request
     */
    void process_msho_req (Packet *req);

    /**
     * Process handover indication
     * @param p The indication
     */
    void process_ho_ind (Packet *p);

    /**
     * Send a neighbor advertisement message
     */
    void send_nbr_adv ();

    /**
     * Add a new timer 17 for client
     * @param index The client address
     */
    void addtimer17 (int index);

    /**
     * Cancel and remove the timer17 associated with the node
     * @param index The client address
     */
    void removetimer17 (int index);


    /**
     * Pointer to the head of the list of nodes that should
     * perform registration
     */
    struct t17element t17_head_;

    /**
     * Pointer to the head of the list of clients
     */
    struct new_client_t *cl_head_;

    /**
     * Pointer to the tail of the list of clients
     */
    struct new_client_t *cl_tail_;

    /**
     * The index of the last SS that had bandwidth allocation
     */
    int bw_node_index_;

    /**
     * The node that had the last bandwidth allocation
     */
    PeerNode *bw_peer_;

    /**
     * Timer for DCD
     */
    WimaxDCDTimer *dcdtimer_;

    /**
     * Timer for UCD
     */
    WimaxUCDTimer *ucdtimer_;

    /**
     * Timer for MOB-NBR_ADV messages
     */
    WimaxMobNbrAdvTimer *nbradvtimer_;

    /**
     * List of station in scanning
     */
    struct scanningStation scan_stations_;

    /**
     * The Wimax control for RS synchronization
     */
    WimaxCtrlAgent *ctrlagent_;

    /**
     * List of the upcoming Fast Ranging allocation
     */
    struct fastRangingInfo fast_ranging_head_;

    /**
    * Indicates if scan has been requested
    */
    bool scan_flag_;

    /**
     * Default DIUC to use.
     */
    u_char default_diuc_;

};

#endif //MAC802_16RS_H

