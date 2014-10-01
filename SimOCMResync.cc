


#include <time.h>
#include <pthread.h>
#include <iostream>
#include <iomanip>


using namespace std;


void print_time()
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    cout << ts.tv_sec % 100000 << "." << setfill('0') << setw(3) << ts.tv_nsec / 1000000;
}



class Switch;

class FPGA
{
public:
    FPGA(Switch *s)
    {
        optoswitch = s;
        last_port = cur_port = next_port = 0;
    };

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
    OCM(FPGA *f): port_scanned(0), fpga(f) {};

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
    for (;;)
    {
        data_valid = false;
        data_ready = false;

        //sweep the spectrum
        //get port number
        usleep(10000);  //10ms to sweep the spectrum

        //process the data
        usleep(100000);
        data_valid = true;
        usleep(100000);

        //data is ready
        data_ready = true;
        print_time();
        cout << ": OCM : data_ready for port " << port_scanned << endl;
        fpga->data_ready_interrupt();

        usleep(500000);
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
    print_time();
    cout << ": FPGA : last=" << last_port << ", cur=" << cur_port << ", next=" <<
        next_port << ". interrupt to software." << endl;

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
    print_time();
    cout << ": Switch : strobed to port " << port << endl;
}



int main()
{
    Switch *sw = new Switch();
    FPGA *fpga = new FPGA(sw);
    OCM *ocm = new OCM(fpga);

    ocm->start();

    pthread_join(ocm->thread, NULL);
    return 0;
}