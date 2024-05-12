#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cop4610t- Group 3");
MODULE_DESCRIPTION("Kernel module for our groups elevator");

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL

static struct proc_dir_entry *elevator_entry;

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

// Enumerations
enum elevator_state
{
    OFFLINE,
    IDLE,
    LOADING,
    UP,
    DOWN
};

// Passenger struct
struct Passenger
{
    char type;
    int destination_floor;
    int starting_floor;
    int weight;
    struct list_head list;
};

// Elevator struct
struct Elevator
{
    enum elevator_state current_state;
    int deactivating;
    int initialized;
    int current_floor;
    int weight;
    int num_passengers;
    int direction;
    int num_serviced;
    struct list_head elevator_list;
    struct task_struct *thread;
    struct mutex elevator_mutex;
};

// Floors struct
struct Floors
{
    int initialized;
    int curr_waiting[5];
    int num_passengers_waiting;
    struct list_head floor_lists[5];
    struct mutex floors_mutex;
};

/*===========================================================================*/
/*=============================Function Headers==============================*/
/*===========================================================================*/

// Syscall Functions
int start_elevator(void);
int issue_request(int start_floor, int destination_floor, int type);
int stop_elevator(void);

// // Helper Functions
void initialize_elevator(struct Elevator *elevator);
void initialize_floors(struct Floors *floors);
int activate_elevator(void *_elevator);

// Passenger Functions
int create_passenger(int type, int destination_floor, int starting_floor);

// Un/Loading Functions
int can_load_passenger(struct Elevator *elevator_thread);
void load_passenger(struct Elevator *elevator_thread);
int can_unload_passenger(struct Elevator *elevator_thread);
void unload_passenger(struct Elevator *elevator_thread);

// Elevator Movement
void move_elevator(struct Elevator *elevator_thread);

// Proc File Function
static ssize_t elevator_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);

// Cleanup Functions
void clean_up(struct Elevator *elevator_thread, struct Floors *floors);

// global variables
struct Elevator elevator;
struct Floors floors;

/*===========================================================================*/
/*=============================Syscall Functions=============================*/
/*===========================================================================*/

int start_elevator(void)
{
    if (elevator.initialized)
    {
        if (elevator.deactivating)
        {
            elevator.deactivating = 0;
        }
        return 1;
    }
    else
    {
        mutex_init(&elevator.elevator_mutex);
        initialize_elevator(&elevator);
        elevator.thread = kthread_run(activate_elevator, &elevator, "thread elevator\n");
    }

    return 0;
}

int issue_request(int start_floor, int destination_floor, int type)
{
    // Validate input
    if (start_floor < 1 || start_floor > 5 || destination_floor < 1 || destination_floor > 5 || type < 0 || type > 3)
    {
        return 1;
    }

    return create_passenger(type, destination_floor, start_floor);
}

int stop_elevator(void)
{
    mutex_lock_interruptible(&elevator.elevator_mutex);
    if (elevator.deactivating)
    {
        mutex_unlock(&elevator.elevator_mutex);
        return 1;
    }
    else
    {
        elevator.deactivating = 1;
    }

    mutex_unlock(&elevator.elevator_mutex);
    return 0;
}

/*===========================================================================*/
/*=============================Helper Functions==============================*/
/*===========================================================================*/

void initialize_elevator(struct Elevator *elevator)
{
    mutex_lock_interruptible(&elevator->elevator_mutex);
    elevator->current_state = 1;
    elevator->weight = 0;
    elevator->num_passengers = 0;
    INIT_LIST_HEAD(&elevator->elevator_list);
    elevator->thread = NULL;
    elevator->initialized = 1;
    elevator->deactivating = 0;
    mutex_unlock(&elevator->elevator_mutex);
}

void initialize_floors(struct Floors *floors)
{
    mutex_lock_interruptible(&floors->floors_mutex);
    for (int i = 0; i < 5; ++i)
    {
        INIT_LIST_HEAD(&floors->floor_lists[i]);
        floors->curr_waiting[i] = 0;
    }
    floors->initialized = 1;
    floors->num_passengers_waiting = 0;
    mutex_unlock(&floors->floors_mutex);
}

int activate_elevator(void *_elevator)
{
    struct Elevator *elevator_thread = (struct Elevator *)_elevator;
    while (!kthread_should_stop())
    {
        move_elevator(elevator_thread);
    }
    return 0;
}

/*===========================================================================*/
/*============================Passenger Functions============================*/
/*===========================================================================*/

int create_passenger(int type, int destination_floor, int starting_floor)
{
    struct Passenger *passenger = kmalloc(sizeof(struct Passenger), GFP_KERNEL);
    if (!passenger)
    {
        // Handle allocation failure
        return 1;
    }

    switch (type)
    {
    case 0:
        passenger->type = 'P';
        passenger->weight = 100;
        break;
    case 1:
        passenger->type = 'L';
        passenger->weight = 150;
        break;
    case 2:
        passenger->type = 'B';
        passenger->weight = 200;
        break;
    case 3:
        passenger->type = 'V';
        passenger->weight = 50;
        break;
    }

    passenger->destination_floor = destination_floor;
    passenger->starting_floor = starting_floor;

    mutex_lock_interruptible(&floors.floors_mutex);
    list_add_tail(&passenger->list, &floors.floor_lists[passenger->starting_floor - 1]);
    floors.curr_waiting[passenger->starting_floor - 1]++;
    floors.num_passengers_waiting++;
    mutex_unlock(&floors.floors_mutex);

    return 0;
}

/*===========================================================================*/
/*===========================Un/Loading Functions============================*/
/*===========================================================================*/

int can_load_passenger(struct Elevator *elevator_thread)
{
    struct Passenger *temp_node;

    if (floors.curr_waiting[elevator_thread->current_floor - 1] != 0)
    {
        list_for_each_entry(temp_node, &floors.floor_lists[elevator_thread->current_floor - 1], list)
        {
            if (elevator_thread->num_passengers < 5 &&
                elevator_thread->weight + temp_node->weight <= 700)
            {
                return 1;
            }
        }
    }

    return 0;
}

void load_passenger(struct Elevator *elevator_thread)
{
    struct Passenger *first, *second;

    list_for_each_entry_safe(first, second, &floors.floor_lists[elevator_thread->current_floor - 1], list)
    {
        if (elevator_thread->num_passengers < 5 && elevator_thread->weight + first->weight <= 700)
        {
            // remove the passenger from the floors list
            list_del(&first->list);

            // add the passenger to elevator list
            list_add_tail(&first->list, &elevator_thread->elevator_list);

            elevator_thread->weight += first->weight;
            elevator_thread->num_passengers++;
            floors.num_passengers_waiting--;
            floors.curr_waiting[elevator_thread->current_floor - 1]--;
        }
    }
}

int can_unload_passenger(struct Elevator *elevator_thread)
{
    struct Passenger *temp_node;

    list_for_each_entry(temp_node, &elevator_thread->elevator_list, list)
    {
        if (temp_node->destination_floor == elevator_thread->current_floor)
        {
            return 1;
        }
    }

    return 0;
}

void unload_passenger(struct Elevator *elevator_thread)
{
    struct Passenger *first, *second;
    int temp_weight;

    // iterate over each passenger currently on the elevator
    list_for_each_entry_safe(first, second, &elevator_thread->elevator_list, list)
    {
        if (first->destination_floor == elevator_thread->current_floor)
        {
            temp_weight = first->weight;
            list_del(&first->list);
            kfree(first);

            elevator_thread->num_serviced++;
            elevator_thread->num_passengers--;
            elevator_thread->weight -= temp_weight;
        }
    }
}

/*===========================================================================*/
/*=============================Elevator Movement=============================*/
/*===========================================================================*/

void move_elevator(struct Elevator *elevator_thread)
{
    switch (elevator_thread->current_state)
    {
    case IDLE:
        ssleep(1);

        if (elevator_thread->deactivating)
        {
            mutex_lock_interruptible(&elevator_thread->elevator_mutex);
            elevator_thread->current_state = OFFLINE;
            elevator_thread->initialized = 0;
            mutex_unlock(&elevator_thread->elevator_mutex);
            kthread_stop(elevator_thread->thread);
        }
        else
        {
            mutex_lock_interruptible(&floors.floors_mutex);
            mutex_lock_interruptible(&elevator_thread->elevator_mutex);
            if (floors.num_passengers_waiting > 0)
            {
                if (can_load_passenger(elevator_thread) || can_unload_passenger(elevator_thread))
                {
                    elevator_thread->current_state = LOADING;
                }
                else
                {
                    if ((elevator_thread->direction && elevator_thread->current_floor != 5) ||
                        (!elevator_thread->direction && elevator_thread->current_floor == 1))
                    {
                        elevator_thread->current_state = UP;
                        elevator_thread->current_floor++;
                        elevator_thread->direction = 1;
                    }
                    else
                    {
                        elevator_thread->current_state = DOWN;
                        elevator_thread->current_floor--;
                        elevator_thread->direction = 0;
                    }
                }
            }
            mutex_unlock(&elevator_thread->elevator_mutex);
            mutex_unlock(&floors.floors_mutex);
        }

        break;

    case LOADING:
        ssleep(1);

        mutex_lock_interruptible(&elevator_thread->elevator_mutex);
        mutex_lock_interruptible(&floors.floors_mutex);
        if (can_unload_passenger(elevator_thread))
        {
            unload_passenger(elevator_thread);
        }

        if (can_load_passenger(elevator_thread) && !elevator_thread->deactivating)
        {
            load_passenger(elevator_thread);
        }

        if (elevator_thread->num_passengers > 0 || (floors.num_passengers_waiting > 0 && !elevator_thread->deactivating))
        {
            if (elevator_thread->direction)
            {
                if (elevator_thread->current_floor != 5)
                {
                    elevator_thread->current_floor++;
                    elevator_thread->current_state = UP;
                }
                else
                {
                    elevator_thread->current_state = DOWN;
                    elevator_thread->current_floor--;
                    elevator_thread->direction = 0;
                }
            }
            else
            {
                if (elevator_thread->current_floor != 1)
                {
                    elevator_thread->current_floor--;
                    elevator_thread->current_state = DOWN;
                }
                else
                {
                    elevator_thread->current_state = UP;
                    elevator_thread->current_floor++;
                    elevator_thread->direction = 1;
                }
            }
        }
        else
        {
            elevator_thread->current_state = IDLE;
        }

        mutex_unlock(&elevator_thread->elevator_mutex);
        mutex_unlock(&floors.floors_mutex);
        break;

    case UP:
        ssleep(2);

        mutex_lock_interruptible(&elevator_thread->elevator_mutex);
        mutex_lock_interruptible(&floors.floors_mutex);

        if ((can_load_passenger(elevator_thread) && !elevator_thread->deactivating) ||
            can_unload_passenger(elevator_thread))
        {
            elevator_thread->current_state = LOADING;
        }
        else
        {
            if ((floors.num_passengers_waiting > 0 && !elevator_thread->deactivating) || elevator_thread->num_passengers > 0)
            {
                if (elevator_thread->current_floor != 5)
                {
                    elevator_thread->current_floor++;
                }
                else
                {
                    elevator_thread->current_state = DOWN;
                    elevator_thread->current_floor--;
                    elevator_thread->direction = 0;
                }
            }
            else
            {
                elevator_thread->current_state = IDLE;
            }
        }

        mutex_unlock(&elevator_thread->elevator_mutex);
        mutex_unlock(&floors.floors_mutex);
        break;

    case DOWN:
        ssleep(2);

        mutex_lock_interruptible(&elevator_thread->elevator_mutex);
        mutex_lock_interruptible(&floors.floors_mutex);
        if ((can_load_passenger(elevator_thread) && !elevator_thread->deactivating) ||
            can_unload_passenger(elevator_thread))
        {
            elevator_thread->current_state = LOADING;
        }
        else
        {
            if ((floors.num_passengers_waiting > 0 && !elevator_thread->deactivating) || elevator_thread->num_passengers > 0)
            {
                if (elevator_thread->current_floor != 1)
                {
                    elevator_thread->current_floor--;
                }
                else
                {
                    elevator_thread->current_state = UP;
                    elevator_thread->current_floor++;
                    elevator_thread->direction = 1;
                }
            }
            else
            {
                elevator_thread->current_state = IDLE;
            }
        }

        mutex_unlock(&elevator_thread->elevator_mutex);
        mutex_unlock(&floors.floors_mutex);
        break;

    default:
        ssleep(1);
        break;
    }
}

/*===========================================================================*/
/*============================Proc File Function=============================*/
/*===========================================================================*/

static ssize_t elevator_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    mutex_lock_interruptible(&elevator.elevator_mutex);
    mutex_lock_interruptible(&floors.floors_mutex);

    char buf[10000];
    int len = 0;
    struct Passenger *passenger;
    struct Passenger *elevator_temp;

    switch (elevator.current_state)
    {
    case OFFLINE:
        len = sprintf(buf, "Elevator state: %s\n", "OFFLINE");
        break;
    case IDLE:
        len = sprintf(buf, "Elevator state: %s\n", "IDLE");
        break;
    case LOADING:
        len = sprintf(buf, "Elevator state: %s\n", "LOADING");
        break;
    case UP:
        len = sprintf(buf, "Elevator state: %s\n", "UP");
        break;
    case DOWN:
        len = sprintf(buf, "Elevator state: %s\n", "DOWN");
        break;
    }

    len += sprintf(buf + len, "Current floor: %d\n", elevator.current_floor);
    len += sprintf(buf + len, "Current load: %d\n", elevator.weight);
    len += sprintf(buf + len, "Elevator status: ");

    if (elevator.initialized)
    {
        list_for_each_entry(elevator_temp, &elevator.elevator_list, list)
        {
            len += sprintf(buf + len, "%c%d ", elevator_temp->type,
                           elevator_temp->destination_floor);
        }
    }

    len += sprintf(buf + len, "\n\n");

    for (int floor_counter = 4; floor_counter >= 0; floor_counter--)
    {
        if (elevator.current_floor == floor_counter + 1)
        {
            len += sprintf(buf + len, "[*] Floor %d: %d", floor_counter + 1,
                           floors.curr_waiting[floor_counter]);
        }
        else
        {
            len += sprintf(buf + len, "[ ] Floor %d: %d", floor_counter + 1,
                           floors.curr_waiting[floor_counter]);
        }
        if (floors.initialized)
        {
            list_for_each_entry(passenger, &floors.floor_lists[floor_counter], list)
            {
                len += sprintf(buf + len, " %c%d", passenger->type,
                               passenger->destination_floor);
            }
        }

        len += sprintf(buf + len, "\n");
    }

    len += sprintf(buf + len, "\nNumber of passengers: %d\n",
                   elevator.num_passengers);
    len += sprintf(buf + len, "Number of passengers waiting: %d\n",
                   floors.num_passengers_waiting);
    len += sprintf(buf + len, "Number of passengers serviced: %d\n",
                   elevator.num_serviced);
    // you can finish the rest.

    mutex_unlock(&elevator.elevator_mutex);
    mutex_unlock(&floors.floors_mutex);

    return simple_read_from_buffer(ubuf, count, ppos, buf, len); // better than copy_from_user
}

/*===========================================================================*/
/*=============================Cleanup Functions=============================*/
/*===========================================================================*/

void clean_up(struct Elevator *elevator_thread, struct Floors *floors)
{
    struct Passenger *ele_pass1, *ele_pass2;
    struct Passenger *floor_pass1, *floor_pass2;

    mutex_lock_interruptible(&elevator_thread->elevator_mutex);
    if (elevator_thread->num_passengers > 0)
    {
        if (!list_empty(&elevator_thread->elevator_list))
        {
            list_for_each_entry_safe(ele_pass1, ele_pass2, &elevator_thread->elevator_list, list)
            {
                // Remove the element from the list before freeing it
                list_del(&ele_pass1->list);

                // Free the memory associated with the element
                kfree(ele_pass1);
            }
        }
        elevator_thread->num_passengers = 0;
    }

    mutex_unlock(&elevator_thread->elevator_mutex);

    mutex_lock_interruptible(&floors->floors_mutex);
    if (floors->initialized)
    {
        for (int i = 0; i < 5; ++i)
        {
            if (!list_empty(&floors->floor_lists[i]))
            {
                list_for_each_entry_safe(floor_pass1, floor_pass2, &floors->floor_lists[i], list)
                {
                    // Remove the element from the list before freeing it
                    list_del(&floor_pass1->list);

                    // Free the memory associated with the element
                    kfree(floor_pass1);
                }
            }
        }
        floors->initialized = 0;
    }
}

/*===========================================================================*/
/*=============================Module Functions==============================*/
/*===========================================================================*/

static const struct proc_ops elevator_fops = {
    .proc_read = elevator_read,
};

static int __init elevator_init(void)
{
    STUB_start_elevator = start_elevator;
    STUB_issue_request = issue_request;
    STUB_stop_elevator = stop_elevator;

    elevator_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &elevator_fops);
    if (!elevator_entry)
    {
        return -ENOMEM;
    }

    mutex_init(&floors.floors_mutex);
    initialize_floors(&floors);

    // elevator initialization
    elevator.current_floor = 1;
    elevator.direction = 1;
    elevator.initialized = 0;
    elevator.num_serviced = 0;

    return 0;
}

static void __exit elevator_exit(void)
{
    // Unlink proc entry
    STUB_start_elevator = NULL;
    STUB_issue_request = NULL;
    STUB_stop_elevator = NULL;

    // Mutex cleanup
    mutex_unlock(&elevator.elevator_mutex);
    mutex_unlock(&floors.floors_mutex);
    mutex_destroy(&elevator.elevator_mutex);
    mutex_destroy(&floors.floors_mutex);

    // Memory cleanup
    clean_up(&elevator, &floors);
    proc_remove(elevator_entry);
}

module_init(elevator_init);
module_exit(elevator_exit);
