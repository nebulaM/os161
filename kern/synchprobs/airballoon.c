/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NROPES 16
static int ropes_left = NROPES;

// Data structures for rope mappings

/* Implement this! */
struct rope{
    struct lock *lock;
    unsigned int index;
    volatile unsigned int stake;
    volatile bool is_cut;
};

static struct rope* create_rope(const char *rope_name, unsigned int index){
    struct rope *my_rope;
    my_rope=kmalloc(sizeof(struct rope));
    KASSERT(my_rope != NULL);
    my_rope->lock=lock_create(rope_name);
    KASSERT(my_rope->lock != NULL);
    my_rope->is_cut=false;
    my_rope->index=index;
    my_rope->stake=index;
    return my_rope;
}

static void destroy_rope(struct rope *my_rope){
    KASSERT(my_rope != NULL);
    lock_destroy(my_rope->lock);
    kfree(my_rope);
}

// Synchronization primitives

/* Implement this! */

static struct rope *ropes[NROPES];

static struct semaphore *exit_balloon_sem;
static struct semaphore *exitsem;

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 */

static
void
dandelion(void *p, unsigned long arg)
{
    (void)p;
    (void)arg;

    kprintf("Dandelion thread starting\n");

    // Implement this function
    while(ropes_left>0){
        //loop over all ropes
        for(unsigned int i=0; i<NROPES; ++i){
            if(!ropes[i]->is_cut){
                lock_acquire(ropes[i]->lock);
                if(!ropes[i]->is_cut){
                    ropes[i]->is_cut=true;
                    ropes_left--;
                    kprintf("Dandelion severed rope %u\n",
                      ropes[i]->index);
                }
                lock_release(ropes[i]->lock);
                thread_yield();
                break;
            }
        }
    }

    kprintf("Dandelion thread done\n");
    V(exit_balloon_sem);
}

static
void
marigold(void *p, unsigned long arg)
{
    (void)p;
    (void)arg;
    kprintf("Marigold thread starting\n");

    // Implement this function
    while(ropes_left>0){
        //loop over all ropes
        for(unsigned int i=0; i<NROPES; ++i){
            if(!ropes[i]->is_cut){
                lock_acquire(ropes[i]->lock);
                if(!ropes[i]->is_cut){
                    ropes[i]->is_cut=true;
                    ropes_left--;
                    kprintf("Marigold severed rope %u from stake %u\n",
                      ropes[i]->index,ropes[i]->stake);
                }
                lock_release(ropes[i]->lock);
                thread_yield();
                break;
            }
        }
    }

    kprintf("Marigold thread done\n");
    V(exit_balloon_sem);
}

static
void
flowerkiller(void *p, unsigned long arg)
{
    (void)p;
    (void)arg;

    kprintf("Lord FlowerKiller thread starting\n");

    // Implement this function
    while(ropes_left>0){
        struct rope *this_rope=ropes[random()%NROPES];
        if(!this_rope->is_cut){
            lock_acquire(this_rope->lock);
            //double check in case the rope is cut while we wait for lock_acquire
            if(!this_rope->is_cut){
                //store old stake after we get the clock
                //this_rope->stake is volatile, read from it is expensive
                unsigned int old_stake=this_rope->stake;
                unsigned int new_stake=random()%NROPES;
                if(old_stake==new_stake){
                    if(old_stake==NROPES-1){
                        new_stake=(NROPES-1)/3;
                    }else if(this_rope->stake==0){
                        new_stake=(NROPES-1)/2;
                    }else{
                        new_stake=old_stake++;
                    }
                }
                this_rope->stake=new_stake;

                kprintf("Lord FlowerKiller switched rope %u from stake %u to stake %u\n",
                  this_rope->index,old_stake,new_stake);
            }
            lock_release(this_rope->lock);
            thread_yield();
        }
    }

    kprintf("Lord FlowerKiller thread done\n");
    //inc semaphore to indicate this thread is done
    V(exit_balloon_sem);
}

static
void
balloon(void *p, unsigned long arg)
{
    (void)p;
    (void)arg;

    kprintf("Balloon thread starting\n");

    // Implement this function
    // simply wait other threads done
    for(int i=0;i<3;++i){
        P(exit_balloon_sem);
    }
    kprintf("Balloon thread done\n");
    V(exitsem);
}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{

    int err = 0;

    (void)nargs;
    (void)args;
    ropes_left = NROPES;

    for(unsigned int i=0;i<NROPES;++i){
        ropes[i]=create_rope("rope", i);
    }

    exitsem=sem_create("exitsem",0);
    exit_balloon_sem=sem_create("exit_balloon_sem",0);

    err = thread_fork("Marigold Thread",
      NULL, marigold, NULL, 0);
    if(err)
        goto panic;

    err = thread_fork("Dandelion Thread",
      NULL, dandelion, NULL, 0);
    if(err)
        goto panic;

    err = thread_fork("Lord FlowerKiller Thread",
      NULL, flowerkiller, NULL, 0);
    if(err)
        goto panic;

    err = thread_fork("Air Balloon",
      NULL, balloon, NULL, 0);
    if(err)
        goto panic;

    goto done;
panic:
    panic("airballoon: thread_fork failed: %s)\n",
      strerror(err));

done:
    P(exitsem);

    sem_destroy(exitsem);
    sem_destroy(exit_balloon_sem);

    for(int i=0;i<NROPES;++i){
        destroy_rope(ropes[i]);
    }

    kprintf("Main thread done\n");

    return 0;
}
