


#include <time.h>
#include <pthread.h>
#include <iostream>
#include <iomanip>


using namespace std;


class OCM 
{
public:
    OCM() {};

    void start()
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&thread, &attr, &OCM::startthread, static_cast<void *>(this));
    }

    pthread_t thread;

private:
    void mainloop()
    {
        struct timespec ts;

        for (;;)
        {
            clock_gettime(CLOCK_REALTIME, &ts);
            cout << ts.tv_sec % 100000 << "." << setfill('0') << setw(3) << ts.tv_nsec / 1000000 << ": in mainloop" << endl;
            usleep(500000);
        }
    }

    static void *startthread(void *arg)
    {
        OCM *ocm = static_cast<OCM *>(arg);
        ocm->mainloop();
    }
};


int main()
{
    OCM ocm;

    ocm.start();

    pthread_join(ocm.thread, NULL);
    return 0;
}