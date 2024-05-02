#include "channel.h"

// Creates a new channel with the provided size and returns it to the caller
// A 0 size indicates an unbuffered channel, whereas a positive size indicates a buffered channel
chan_t* channel_create(size_t size) {
    chan_t* channel = (chan_t*)malloc(sizeof(chan_t)); // Memory allocation for new channel struct
    
    if (channel == NULL) {
        return NULL; // Check if memory allocation failed
    }

    // Initialize the buffer
    channel->buffer = buffer_create(size); // Memory allocation for creating buffer under channel

    // Initialize mutex and conditions
    pthread_mutex_init(&channel->mutex, NULL);
    pthread_mutex_init(&channel->send_list_mutex, NULL);
    pthread_mutex_init(&channel->receive_list_mutex, NULL);
    pthread_cond_init(&channel->send_condition, NULL);
    pthread_cond_init(&channel->receive_condition, NULL);
    
    // Initialize the lists for select
    channel->send_list = list_create();
    channel->receive_list = list_create();
    
    // Initialize close flag
    channel->closed = false;

    return channel;
}

// Writes data to the given channel
// This can be both a blocking call i.e., the function only returns on a successful completion of send (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is full (blocking = false)
// In case of the blocking call when the channel is full, the function waits till the channel has space to write the new data
// Returns SUCCESS for successfully writing data to the channel,
// WOULDBLOCK if the channel is full and the data was not added to the buffer (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_send(chan_t* channel, void* data, bool blocking)
{
    if (channel == NULL) {
        return OTHER_ERROR; // Taking invalid arguments
    }
    
    // Lock the buffer
    if (blocking) {
        pthread_mutex_lock(&channel->mutex);
    } else {
    	// Use trylock instead of lock in non blocking
        if (pthread_mutex_lock(&channel->mutex) != 0) {
            return WOULDBLOCK;
        }
    }

    // Initial check if the channel is closed
    if (channel->closed) {
        pthread_mutex_unlock(&channel->mutex);
        return CLOSED_ERROR;
    }

    // Perform checks for space in the buffer
    if (blocking) {
        // Blocking
        while (buffer_capacity(channel->buffer) - buffer_current_size(channel->buffer) == 0) {
            // Perform wait on send to wait for space in channel for data
            pthread_cond_wait(&channel->send_condition, &channel->mutex);
            
            // Check if the channel is closed while channel_send is running
            if (channel->closed) {
                pthread_mutex_unlock(&channel->mutex);
                return CLOSED_ERROR;
            }
        }
    } else {
    	// Non-blocking
        if (buffer_capacity(channel->buffer) - buffer_current_size(channel->buffer) == 0) {
            pthread_mutex_unlock(&channel->mutex);
            return WOULDBLOCK;
        }
    }

    // Perform the send operation
    buffer_add(data, channel->buffer);

    // Signal that there is a filled slot in the buffer
    pthread_cond_signal(&channel->receive_condition);
    
    // Unlock the mutex
    pthread_mutex_unlock(&channel->mutex);
    
    // Lock mutex of receive list
    pthread_mutex_lock(&channel->receive_list_mutex);
    
    if (list_count(channel->receive_list) != 0) {
        list_foreach(channel->receive_list, (void *) sem_post); // Notify receive list that there is filled slot in buffer (Channel is available to receive)
    }
    
    // Unlock mutex of receive list
    pthread_mutex_unlock(&channel->receive_list_mutex);
    
    return SUCCESS;
}

// Reads data from the given channel and stores it in the function’s input parameter, data (Note that it is a double pointer).
// This can be both a blocking call i.e., the function only returns on a successful completion of receive (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is empty (blocking = false)
// In case of the blocking call when the channel is empty, the function waits till the channel has some data to read
// Returns SUCCESS for successful retrieval of data,
// WOULDBLOCK if the channel is empty and nothing was stored in data (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_receive(chan_t* channel, void** data, bool blocking)
{
    if (channel == NULL) {
        return OTHER_ERROR; // Taking invalid arguments
    }
    
    // Lock the buffer
    if (blocking) {
        pthread_mutex_lock(&channel->mutex);
    } else {
    	// Use trylock instead of lock in non blocking
        if (pthread_mutex_lock(&channel->mutex) != 0) {
            return WOULDBLOCK;
        }
    }

    // Initial check if the channel is closed
    if (channel->closed) {
        pthread_mutex_unlock(&channel->mutex);
        return CLOSED_ERROR;
    }

    // Perform checks for data in the buffer
    if (blocking) {
    	// Blocking
        while (buffer_current_size(channel->buffer) == 0) {
            // Perform wait on receive to wait for data present
            pthread_cond_wait(&channel->receive_condition, &channel->mutex);
            
            // Check if the channel is closed while channel_receive is running
            if (channel->closed) {
                pthread_mutex_unlock(&channel->mutex);
                return CLOSED_ERROR;
            }
        }
    } else {
    	// Non blocking
        if (buffer_current_size(channel->buffer) == 0) {
            pthread_mutex_unlock(&channel->mutex);
            return WOULDBLOCK;
        }
    }

    // Perform the receive operation
    *data = buffer_remove(channel->buffer);

    // Signal that there is an empty slot in the buffer
    pthread_cond_signal(&channel->send_condition);
    
    // Unlock the mutex
    pthread_mutex_unlock(&channel->mutex);
    
    // Lock mutex for send list
    pthread_mutex_lock(&channel->send_list_mutex);
    
    if (list_count(channel->send_list) != 0) {
        list_foreach(channel->send_list, (void *) sem_post); // Notify send list that there is empty slot in buffer (Channel is available to send)
    }
    // Lock mutex for send list
    pthread_mutex_unlock(&channel->send_list_mutex);
    
    return SUCCESS;
}

// Closes the channel and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the channel is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns SUCCESS if close is successful,
// CLOSED_ERROR if the channel is already closed, and
// OTHER_ERROR in any other error case
enum chan_status channel_close(chan_t* channel)
{
    if (channel == NULL) {
        return OTHER_ERROR; // Taking invalid arguments
    }
    
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);
    
    if (channel->closed == true) {
        pthread_mutex_unlock(&channel->mutex);
        return CLOSED_ERROR; // Called close on closed channel
    }


    // Set the closed flag
    channel->closed = true;
    

    // Broadcast to wake up any waiting threads
    pthread_cond_broadcast(&channel->send_condition);
    pthread_cond_broadcast(&channel->receive_condition);
    
    // Unlock the mutex
    pthread_mutex_unlock(&channel->mutex);
    
    // Update send list
    // Lock mutex for send list
    pthread_mutex_lock(&channel->send_list_mutex);
    
    if (list_count(channel->send_list) != 0) {
        list_foreach(channel->send_list, (void *) sem_post); // Notify send list that channel is closed
    }
    // Unlock mutex for send list
    pthread_mutex_unlock(&channel->send_list_mutex);
    
    // Update receive list
    // Lock mutex for receive list
    pthread_mutex_lock(&channel->receive_list_mutex);
    
    if (list_count(channel->receive_list) != 0) {
        list_foreach(channel->receive_list, (void *) sem_post); // Notify receive list that channel is closed
    }
    // Unlock mutex for receive list
    pthread_mutex_unlock(&channel->receive_list_mutex);

    return SUCCESS;
    
}

// Frees all the memory allocated to the channel
// The caller is responsible for calling channel_close and waiting for all threads to finish their tasks before calling channel_destroy
// Returns SUCCESS if destroy is successful,
// DESTROY_ERROR if channel_destroy is called on an open channel, and
// OTHER_ERROR in any other error case
enum chan_status channel_destroy(chan_t* channel)
{
    if (channel == NULL) {
        return OTHER_ERROR; // Taking invalid arguments
    }
    
    if (channel->closed == false) {
    	return DESTROY_ERROR; // Called destroy on open channel
    }

    // Free the buffer
    buffer_free(channel->buffer);
    
    // Destroy mutex and cond
    pthread_mutex_destroy(&channel->mutex);
    pthread_mutex_destroy(&channel->send_list_mutex);
    pthread_mutex_destroy(&channel->receive_list_mutex);
    pthread_cond_destroy(&channel->send_condition);
    pthread_cond_destroy(&channel->receive_condition);
    
    // Destroy lists
    list_destroy(channel->send_list);
    list_destroy(channel->receive_list);

    // Free the channel structure
    free(channel);

    return SUCCESS;
}

// Takes an array of channels, channel_list, of type select_t and the array length, channel_count, as inputs
// This API iterates over the provided list and finds the set of possible channels which can be used to invoke the required operation (send or receive) specified in select_t
// If multiple options are available, it selects the first option and performs its corresponding action
// If no channel is available, the call is blocked and waits till it finds a channel which supports its required operation
// Once an operation has been successfully performed, select should set selected_index to the index of the channel that performed the operation and then return SUCCESS
// In the event that a channel is closed or encounters any error, the error should be propagated and returned through select
// Additionally, selected_index is set to the index of the channel that generated the error
enum chan_status channel_select(size_t channel_count, select_t* channel_list, size_t* selected_index)
{

    if (channel_count == 0 || channel_list == NULL) {
        return OTHER_ERROR; // Taking invalid arguments
    }
    // Initialize status to return, OTHER_ERROR by default for debug
    enum chan_status status = OTHER_ERROR;
    
    // Initialize local semaphore
    sem_t sem_local;
    if (sem_init(&sem_local, 0, 0) == -1) {
        return OTHER_ERROR; // Semaphore initialization failed
    }
    
    for (size_t i = 0; i < channel_count; i++) {
        // Insert channels into send or receive list based on value of is_send
        if (channel_list[i].is_send == true) {
            // Send channels
            // Lock mutex
            pthread_mutex_lock(&channel_list[i].channel->send_list_mutex);
            
            // Check if channel's semaphore is already in the send list
            if (list_find(channel_list[i].channel->send_list, &sem_local) == NULL) {
                list_insert(channel_list[i].channel->send_list, &sem_local); // Insert its semaphore if not in list
            }
            // Unlock mutex
            pthread_mutex_unlock(&channel_list[i].channel->send_list_mutex);
        } else {
            // Receive channels
            // Lock mutex
            pthread_mutex_lock(&channel_list[i].channel->receive_list_mutex);
            
            // Check if channel is already in the receive list
            if (list_find(channel_list[i].channel->receive_list, &sem_local) == NULL) {
                list_insert(channel_list[i].channel->receive_list, &sem_local); // Insert its semaphore if not in list
            }
            // Unlock mutex
            pthread_mutex_unlock(&channel_list[i].channel->receive_list_mutex);
        }
    }
    while (true) {
        for (size_t i = 0; i < channel_count; i++) {
            // Loop through channel_list to perform send/receive on first available channel in list
            if (channel_list[i].is_send == true) {
                // Do send if can send
                status = channel_send(channel_list[i].channel, channel_list[i].data, false);
            } else {
                // Do receive if not (can receive)
                status = channel_receive(channel_list[i].channel, &channel_list[i].data, false);
            }
        
            if (status != WOULDBLOCK) {
                // Return if status is not WOULDBLOCK
                // set selected_index to channel that perform action
                *selected_index = i;
                for (size_t i = 0; i < channel_count; i++) {
                    // Remove channel from send list since it is not available to send anymore
                    if (channel_list[i].is_send == true) {
                        // Lock mutex
        	        pthread_mutex_lock(&channel_list[i].channel->send_list_mutex);
        	        
        	        // Create node for removing node in list that contains semaphore needed
        	        list_node_t* new_node = list_find(channel_list[i].channel->send_list, &sem_local);
        	        // Remove node if node is in the list
        	        if (new_node != NULL) {
                	    list_remove(channel_list[i].channel->send_list, new_node);
            	        }
            	        // Unlock the mutex
            	        pthread_mutex_unlock(&channel_list[i].channel->send_list_mutex);
            	    // Remove channel from receive list since it is not available to receive anymore
        	    } else {
        	        // Lock mutex
        	        pthread_mutex_lock(&channel_list[i].channel->receive_list_mutex);
        	        
        	        // Create node for removing node in list that contains semaphore needed
        	        list_node_t* new_node = list_find(channel_list[i].channel->receive_list, &sem_local);
        	        // Remove node if node is in the list
            	        if (new_node != NULL) {
                	    list_remove(channel_list[i].channel->receive_list, new_node);
            	        }
            	        // Unlock the mutex
            	        pthread_mutex_unlock(&channel_list[i].channel->receive_list_mutex);
                    }
                }
            // Destroy local semaphore
            sem_destroy(&sem_local);
            
            // Return status
            return status;
        }
    }
    // Wait if status is WOULDBLOCK
    sem_wait(&sem_local);
    }
    // Should never be reached, return OTHER_ERROR
    return OTHER_ERROR;
}
