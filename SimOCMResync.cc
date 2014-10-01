


#include <time.h>
#include <pthread.h>
#include <iostream>
#include <iomanip>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


using namespace std;


void print(string s)
{
    struct timespec ts;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock (&mutex);
    clock_gettime(CLOCK_REALTIME, &ts);
    cout << ts.tv_sec % 100000 << "." << setfill('0') << setw(3) << ts.tv_nsec / 1000000;
    cout << ": " << s << endl;
    pthread_mutex_unlock(&mutex);
}



class Switch;
class Software;

class FPGA
{
public:
    FPGA(Switch *s)
    {
        sw = NULL;
        optoswitch = s;
        last_port = cur_port = next_port = 0;
    };

    Software *sw;

    void data_ready_interrupt();

    int last_port;
    int cur_port;
    int next_port;
    Switch *optoswitch;
};



class Switch 
{
public:
    Switch(): port(0) {};
    void strobe(int new_port);
    int port;
private:
};



class OCM 
{
public:
    OCM(FPGA *f, Switch *sw): port_scanned(0), fpga(f), optoswitch(sw) {};

    void start();

    pthread_t thread;
    bool data_valid;
    bool data_ready;
    int port_scanned;

private:
    void mainloop();

    static void *startthread(void *arg)
    {
        OCM *ocm = static_cast<OCM *>(arg);
        ocm->mainloop();
    }

    FPGA *fpga;
    Switch *optoswitch;
    timer_t timer;
};



class Software 
{
public:
    Software()
    {
        mScanSwitchState = mSwitchState = 0;
    };

    void start();

    OCM *ocm;
    FPGA *fpga;

    void data_ready_interrupt();
    pthread_t thread;

private:
    void mainloop();

    static void *startthread(void *arg)
    {
        Software *sw = static_cast<Software *>(arg);
        sw->mainloop();
    }

    int setOpticalSwitchNext(int );
    int getOpticalSwitchLast(int );

    pthread_cond_t condvar;
    pthread_mutex_t mutex;
    int mScanSwitchState;
    int mSwitchState;
};


/**************************************************************************************/
/* OCM class definitions
/**************************************************************************************/

void OCM::start()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&thread, &attr, &OCM::startthread, static_cast<void *>(this));
}

void OCM::mainloop()
{
    boost::asio::io_service io;
    boost::asio::deadline_timer timer(io, boost::posix_time::milliseconds(500));

    for (;;)
    {
        timer.wait();
        timer.expires_from_now(boost::posix_time::milliseconds(500));

        data_valid = false;
        data_ready = false;

        //sweep the spectrum
        stringstream s;
        port_scanned = optoswitch->port;
        s << " OCM: scanning port " << port_scanned;
        print(s.str());
        usleep(10000);  //10ms to sweep the spectrum

        //process the data
        usleep(100000);
        data_valid = true;
        usleep(100000);

        //data is ready
        data_ready = true;
        s.str("");
        s.clear();
        s << " OCM : data_ready for port " << port_scanned;
        print(s.str());
        fpga->data_ready_interrupt();
    }    
}



/**************************************************************************************/
/* FPGA class definitions
/**************************************************************************************/
void FPGA::data_ready_interrupt()
{
    last_port = cur_port;
    cur_port = next_port;

    //interrupt software
    stringstream s;
    s << " FPGA : last=" << last_port << ", cur=" << cur_port << ", next=" <<
        next_port << ". interrupt to software.";
    print(s.str());
    if (sw)
        sw->data_ready_interrupt();

    //strobe switch with cur_port;
    optoswitch->strobe(cur_port);

}

/**************************************************************************************/
/* Switch class definitions
/**************************************************************************************/
void Switch::strobe(int new_port)
{
    usleep(10000);
    port = new_port;
    stringstream s;
    s << " Switch : strobed to port " << port;
    print(s.str());
}


/**************************************************************************************/
/* Software class definitions
/**************************************************************************************/
void Software::data_ready_interrupt()
{
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&mutex);
}

void Software::start()
{
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&condvar, NULL);    

    //bind this software object to the fpga.
    fpga->sw = this;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&thread, &attr, &Software::startthread, static_cast<void *>(this));
}

void Software::mainloop()
{
    for (;;)
    {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&condvar, &mutex);
        pthread_mutex_unlock(&mutex);

        usleep(10000);  //simulate delay in receiving pulse

        stringstream s;
        s << " Software : received interrupt pulse";
        print(s.str());

        mScanSwitchState = mSwitchState;
        mSwitchState = setOpticalSwitchNext(mSwitchState);
        mScanSwitchState = getOpticalSwitchLast(mScanSwitchState);

        //read the data from the OCM
        usleep(100000);
        int ocm_port = ocm->port_scanned;
        s.str("");
        s.clear();
        s << " Software: expecting data for " << mScanSwitchState << ", received " << ocm_port;
        print(s.str());

        if (ocm_port != mScanSwitchState)
        {
            s.str("");
            s.clear();
            s << " ERROR!!!";
            print(s.str());
            exit(0);
        }
    }
}

int Software::setOpticalSwitchNext(int state)
{
    int currentState, nextState;
    bool resync = false;

    nextState = currentState = state;

    do
    {
        currentState = (currentState + 1) % 5;

        if (currentState != fpga->cur_port)
        {
            stringstream s;
            s << "setOpticalSwitchNext: warm reset detected. resyncing to cur_port: " << fpga->cur_port;
            print(s.str());
            resync = true;
        }
        else
            resync = false;
    } while (resync);

    nextState = (currentState + 1) % 5;
    fpga->next_port = nextState;
    return currentState;
}


int Software::getOpticalSwitchLast(int state)
{
    int lastState = state;
    bool resync = false;

    do
    {
        if (lastState != fpga->last_port)
        {
            stringstream s;
            s << "getOpticalSwitchLast: warm reset detected. resyncing to last_port: " << fpga->last_port;
            print(s.str());
            resync = true;
            lastState = (lastState + 1) % 5;            
        }
        else
            resync = false;
    } while (resync);

    return lastState;
}


int main()
{
    Switch *sw = new Switch();
    FPGA *fpga = new FPGA(sw);
    OCM *ocm = new OCM(fpga, sw);

    ocm->start();

    Software *software = new Software();
    software->ocm = ocm;
    software->fpga = fpga;
    software->start();

    pthread_join(ocm->thread, NULL);
    pthread_join(software->thread, NULL);
    return 0;
}