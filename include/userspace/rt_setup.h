#ifndef RT_SETUP_H
#define RT_SETUP_H

/**
 * @file rt_setup.h
 * @brief Real-time setup utilities for Linux applications
 *
 * This header provides utilities for setting up real-time behavior in Linux
 * applications. It includes functions to configure scheduler parameters,
 * memory locking, and priority settings required for real-time operations.
 *
 * Features:
 * - Memory locking to prevent page faults
 * - FIFO scheduling policy setup
 * - Real-time priority configuration
 *
 * @note These functions require root privileges or proper capabilities to work
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>

/**
 * @brief Sets up real-time environment for the application
 *
 * This function configures the necessary parameters for real-time operation:
 * 1. Locks all current and future memory pages to prevent page faults
 * 2. Sets up FIFO scheduling policy
 * 3. Configures real-time priority
 *
 * @param p Pointer to scheduling parameters structure. Will be initialized
 *          with priority 80 for real-time operations
 *
 * @return EXIT_SUCCESS on successful setup
 * @return EXIT_FAILURE if any operation fails (memory lock, scheduler setup)
 *
 * @note Requires root privileges or CAP_SYS_NICE capability
 */
int rt_setup(struct sched_param *p) {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        return EXIT_FAILURE;
    }  
                 
    if (sched_setscheduler(0, SCHED_FIFO, p) == -1) {
        return EXIT_FAILURE;
    }             

    p = (struct sched_param *) calloc(1, sizeof(struct sched_param));
    if (!p) {
        return EXIT_FAILURE;
    }

    p->sched_priority = 80;
    return EXIT_SUCCESS;
}

/**
 * @brief Cleans up real-time environment settings
 *
 * This function performs cleanup of resources allocated during real-time setup:
 * - Frees allocated scheduler parameter structure
 * - Any additional cleanup required for real-time operation
 *
 * @param p Pointer to scheduler parameters structure to be freed
 *
 * @return EXIT_SUCCESS on successful cleanup
 */
int rt_exit(struct sched_param *p) {
    if (p != NULL) {
        free(p);
    }
    return EXIT_SUCCESS;
}

#endif /* RT_SETUP_H */