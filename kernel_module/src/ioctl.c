// Project 2: Shashank Shekhar, sshekha4; Pranav Gaikwad, pmgaikwa
//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

// defines a task
typedef struct task_node {
    __u64 id;
    struct task_struct *task_pointer;
    struct list_head task_list;
} TaskNode;

// defines a memory object
typedef struct mem_object_node {
    __u64 offset;
    unsigned long kmalloc_area;  // stores pointer to allocated memory area
    struct list_head mem_objects_list;
} ObjectNode;

// defines a container
// contains list of tasks and list of allocated memory objects
typedef struct container_node {
    __u64 id;
    int num_tasks;
    int num_objects;
    TaskNode t_list;
    ObjectNode mem_objects;
    struct mutex mem_lock;   // local lock for operations on mem objects
    struct mutex task_lock;  // local lock for operations on tasks' list
    struct list_head c_list;
} ContainerNode;

// a global lock on list of containers
static DEFINE_MUTEX(container_lock);

// pointer to head of container list
// similar to initializing a linked list
struct list_head container_list_head = LIST_HEAD_INIT(container_list_head);

/**
 * Returns 'container id' in kernel mode from user mode struct
 * @param  user_cmd Command from user mode
 * @return          Container id as an llu
 */
__u64 _get_container_id_in_kernel(struct memory_container_cmd __user *user_cmd) {
    __u64 cid;
    struct memory_container_cmd *buf = (struct memory_container_cmd*)kmalloc(sizeof(*user_cmd), GFP_KERNEL);
    copy_from_user(buf, user_cmd, sizeof(*user_cmd));
    cid = buf->cid;
    kfree(buf);
    return cid;
}

/**
 * Returns 'memory object offset' in kernel mode from user mode struct
 * @param  user_cmd Struct from user mode
 * @return          Memory object offset as an llu
 */
__u64 _get_memory_object_offset_in_kernel(struct memory_container_cmd __user *user_cmd) {
    __u64 oid;
    struct memory_container_cmd *buf = (struct memory_container_cmd*)kmalloc(sizeof(*user_cmd), GFP_KERNEL);
    copy_from_user(buf, user_cmd, sizeof(*user_cmd));
    oid = buf->oid;
    kfree(buf);
    return oid;
}

/**
 * Returns container with given id
 * @param cid Container id
 */      
void* _get_container(__u64 cid) {
    struct list_head *c_pos, *c_q;
    list_for_each_safe(c_pos, c_q, &container_list_head) {
        ContainerNode *temp_container = list_entry(c_pos, ContainerNode, c_list);
        if (temp_container->id == cid) {
            return temp_container;
        }
    }
    return NULL;
}

/**
 * Checks whether given container exists
 * @param  cid Container Id
 * @return     1 if exists, 0 if does not exist
 */
int _container_exists(__u64 cid) {
    ContainerNode* container = (ContainerNode*)_get_container(cid);
    if(container != NULL) {
        return 1;
    }
    return 0;
}

/**
 * Adds new container with given container id to list of containers
 * @param cid Container Id
 */
void _add_new_container(__u64 cid) {
    ContainerNode *new_container;
    new_container = (ContainerNode*)kmalloc(sizeof(ContainerNode), GFP_KERNEL);
    new_container->id = cid;
    new_container->num_tasks = 0;
    // initialize task list and lock 
    mutex_init(&new_container->task_lock);
    INIT_LIST_HEAD(&((new_container->t_list).task_list));
    // initialize memory objects' list and lock
    mutex_init(&new_container->mem_lock);
    INIT_LIST_HEAD(&((new_container->mem_objects).mem_objects_list));
    // add new container to the container list
    list_add(&(new_container->c_list), &container_list_head);
}

/**
 * Registers given container in container list
 * Checks whether given container already exists, 
 * if not, creates a new container & adds it to list
 * @param cid Container Id
 */
void _register_container(__u64 cid) {    
    // Do not create new container if it exists already
    if (_container_exists(cid)) {
        return;
    }    

    mutex_lock(&container_lock);
    _add_new_container(cid);
    mutex_unlock(&container_lock);
}

/**
 * Checks whether given task exists in given container
 * @param  tid task id
 * @param  cid Container id
 * @return     1 if exists, 0 if does not exist
 */
int _task_exists(__u64 cid, __u64 tid) {
    struct list_head *t_pos, *t_q;
    ContainerNode *temp_container = (ContainerNode*)_get_container(cid);

    if (temp_container != NULL) {
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            if (temp_task->id == tid) {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * Adds new task to given container
 * @param cid Container id 
 * @param tid task id
 */
void _add_new_task(__u64 cid, struct task_struct *task_ptr) {
    TaskNode *new_task_node;
    ContainerNode *new_container;

    new_container = (ContainerNode*)_get_container(cid);
    
    if (new_container != NULL) {
        new_task_node = (TaskNode*)kmalloc(sizeof(TaskNode), GFP_KERNEL);
        new_task_node->id = task_ptr->pid;
        new_task_node->task_pointer = task_ptr;
        list_add_tail(&(new_task_node->task_list), &((new_container->t_list).task_list));
        new_container->num_tasks = new_container->num_tasks + 1;    
    }
}

/**
 * Associates given task with given container
 * Checks whether given task already exists in given container, 
 * if not, creates a new entry for given task in the container
 * @param cid Container id 
 * @param tid Task id
 */
void _register_task(__u64 cid, struct task_struct *task_ptr) {
    ContainerNode *container;
    // Do not add new task if it already exists
    if (_task_exists(cid, task_ptr->pid)) {
        return;
    }

    container = (ContainerNode*)_get_container(cid);

    if (container != NULL) {
        mutex_lock(&container->task_lock);
        _add_new_task(cid, task_ptr);
        mutex_unlock(&container->task_lock);
    }
}

/**
 * Finds the container node which contains given task
 * @param  tid Task id
 * @return     Container Node
 */
void* _find_container_containing_task(pid_t tid) {
    ContainerNode *temp_container;
    struct list_head *c_pos, *c_q, *t_pos, *t_q;

    list_for_each_safe(c_pos, c_q, &container_list_head){
        temp_container = list_entry(c_pos, ContainerNode, c_list);
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            if (temp_task->id == tid) {
                return temp_container;
            }
        }
    }
    return NULL;
}

/**
 * Checks whether memory object is already allocated and return the object if present already
 * @param  cid Container Id
 * @return     mem_object if exists, -1 if does not exist
 */
void* _get_memory_object(__u64 offset) {
    struct list_head *o_pos, *o_q;
    ContainerNode *temp_container = (ContainerNode*)_find_container_containing_task(current->pid);

    if (temp_container != NULL) {
        list_for_each_safe(o_pos, o_q, &(temp_container->mem_objects).mem_objects_list) {
            ObjectNode *temp_object = list_entry(o_pos, ObjectNode, mem_objects_list);
            if (temp_object->offset == offset) {
                return temp_object;
            }
        }
    }
    return NULL;
}

/**
 * Adds new object in object list of container
 * @param  cid    Container Id
 * @param  offset Offset of memory object
 * @return       
 */
void _add_new_memory_object(__u64 offset, unsigned long kmalloc_area) {
    ObjectNode *new_object_node;
    ContainerNode *temp_container = (ContainerNode*)_find_container_containing_task(current->pid);
    
    if (temp_container != NULL) {
        new_object_node = (ObjectNode*)kmalloc(sizeof(ObjectNode), GFP_KERNEL);
        new_object_node->offset = offset;
        new_object_node->kmalloc_area = kmalloc_area;
        list_add_tail(&(new_object_node->mem_objects_list), &((temp_container->mem_objects).mem_objects_list));
        temp_container->num_objects = temp_container->num_objects + 1;    
    }
}

/**
 * Removes object with given offset from list of objects of associated container
 * @param  cid    Container Id
 * @param  offset Offset of memory object
 * @return       
 */
void _remove_memory_object(__u64 offset) {
    ContainerNode *temp_container;
    struct list_head *t_pos, *t_q;

    temp_container = (ContainerNode*)_find_container_containing_task(current->pid);

    if (temp_container != NULL) {
            list_for_each_safe(t_pos, t_q, &(temp_container->mem_objects).mem_objects_list) {
            ObjectNode *temp_object = list_entry(t_pos, ObjectNode, mem_objects_list);
            if (temp_object->offset == offset) {
                temp_container->num_objects = temp_container->num_objects - 1;
                list_del(t_pos);
                kfree((void*)temp_object->kmalloc_area);    // free the memory area which was allocated
                kfree(temp_object);
            }
        }
    }
}

/**
 * Removes task from given container 
 * if no tasks remain, deletes the container
 */
void _deregister_task_from_container(pid_t tid) {
    ContainerNode *temp_container;
    struct list_head *c_pos, *c_q, *t_pos, *t_q;

    list_for_each_safe(c_pos, c_q, &container_list_head){
        temp_container = list_entry(c_pos, ContainerNode, c_list);
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            if (temp_task->id == tid) {
                temp_container->num_tasks = temp_container->num_tasks - 1;
                list_del(t_pos);
                kfree(temp_task);
            }
        }
        
        // if there are no more tasks in the container, delete the container
        if (temp_container->num_tasks == 0) {
            list_del(c_pos);
            kfree(temp_container);
        }
    }
}


int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    char *kmalloc_ptr;
    ObjectNode* existing_object;
    unsigned long pfn, kmalloc_area;

    // find out page offset of the current memory object 
    __u64 offset = vma->vm_pgoff;
    // calculate total memory required
    unsigned long total_memory = vma->vm_end - vma->vm_start;

    // update flags
    vma->vm_private_data = filp->private_data;

    // try to find a memory object with same offset    
    existing_object = (ObjectNode*)_get_memory_object(offset);

    ContainerNode* container = (ContainerNode*)_find_container_containing_task(current->pid);
    __u64 cid = 0;
    if (container != NULL) cid = container->id;

    // check if memory object already exists and is allocated 
    if (existing_object != NULL) {
        // use existing memory area, if object with same offset is found
        kmalloc_area = existing_object->kmalloc_area;
        printk("[Found] Container : %lu, Offset : %llu, Memory Location : %llu\n", cid, offset, kmalloc_area);
    } else {
        kmalloc_ptr = (char*)kmalloc(total_memory, GFP_KERNEL);
        kmalloc_area = ((unsigned long)kmalloc_ptr) & PAGE_MASK;
        // update list of memory object
        _add_new_memory_object(offset, kmalloc_area);
        printk("[Not Found] Container : %lu, Offset : %llu, Memory Location : %llu\n", cid, offset, kmalloc_area);
    }
    
    // get pfn for allocated area
    pfn = virt_to_phys((void*)kmalloc_area) >> PAGE_SHIFT;
    // map it
    ret = remap_pfn_range(vma, vma->vm_start, pfn, total_memory, vma->vm_page_prot);

    if (ret) return -EAGAIN;
    if (ret < 0) return -EIO;
    return 0;
}

int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    ContainerNode* container = (ContainerNode*)_find_container_containing_task(current->pid);
    if (container != NULL) {
        mutex_lock(&container->mem_lock);
    }
    return 0;
}

int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    ContainerNode* container = (ContainerNode*)_find_container_containing_task(current->pid);
    if (container != NULL) {
        mutex_unlock(&container->mem_lock);
    }
    return 0;
}

int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    _deregister_task_from_container(current->pid);
    
    return 0;
}

int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
    __u64 cid = _get_container_id_in_kernel(user_cmd);
    
    _register_container(cid);

    _register_task(cid, current);

    return 0;
}

int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
    __u64 offset = _get_memory_object_offset_in_kernel(user_cmd);

    _remove_memory_object(offset);

    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}