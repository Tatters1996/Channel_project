#include "linked_list.h"

// Creates and returns a new list
list_t* list_create()
{
    list_t* list = (list_t*)malloc(sizeof(list_t)); // Memory allocation for new list
    
    if (list != NULL) {
        list->head = NULL; // Initialize the head
        list->count = 0; // Initialize count
    }
    
    return list;
}

// Destroys a list
void list_destroy(list_t* list)
{
   if (list != NULL) {
        // Check if list has any nodes
        // Set temp_node to head of the list
        list_node_t* temp_node = list->head;
        
        while (list->head != NULL) {
            // Loop until there is no nodes left
            list->head = temp_node->next; // Set next node as new head of the list
            free(temp_node); // Delete current head using free
        }
        
        // Delete list using free
        free(list);
   }
}
// Returns beginning of the list
list_node_t* list_begin(list_t* list)
{
   if (list != NULL) {
	return list->head; // Return head in list
    }
    
    //Return NULL if list is empty
    return NULL;
}

// Returns next element in the list
list_node_t* list_next(list_node_t* node)
{
     if (node != NULL) {
    	return node->next; // Return next node of input node
    }
    
    //Return NULL if node is invalid
    return NULL;
}

// Returns data in the given list node
void* list_data(list_node_t* node)
{
    if (node != NULL) {
        return node->data; // Return data of current node
    }
    // Return NULL if node is invalid
    return NULL;
}

// Returns the number of elements in the list
size_t list_count(list_t* list)
{
    if (list != NULL) {
    	return list->count; // Return count if the list is valid
    }
    
    // Return 0 if it contain nothing
    return 0;
}

// Finds the first node in the list with the given data
// Returns NULL if data could not be found
list_node_t* list_find(list_t* list, void* data)
{
    if (list != NULL) {
    // Let temp_node being the head
    list_node_t* temp_node = list->head;
    
    while (temp_node != NULL) {
    	// Search through list if node have data
        if (temp_node->data == data) {
            return temp_node; // Return node if a match is found
        }
        temp_node = temp_node->next; // Go to next node if no match
    }
    // Return NULL if no match in the list
    return NULL;
    }
    
    // Return NULL if list is empty
    return NULL; 
}

// Inserts a new node in the list with the given data
void list_insert(list_t* list, void* data)
{
    if (list != NULL) {
    	// Check if list is valid
        list_node_t* new_node = (list_node_t*)malloc(sizeof(list_node_t)); // Memory allocation for new node
        if (new_node != NULL) {
            // Check if memory allocation failed
            // if not, mark new node as new head
            new_node->data = data; 
            new_node->next = list->head;
            new_node->prev = NULL;

            if (list->head != NULL) {
            	// Check if head contain anything, if so mark prev of head as new node
                list->head->prev = new_node;
            }
	    // if not, just mark new node as head
            list->head = new_node;
            
            // Increment list count
            list->count++;
        }  
    }
}

// Removes a node from the list and frees the node resources
void list_remove(list_t* list, list_node_t* node)
{
    if (list != NULL && node != NULL) {
    	// Check if all arguments are valid
        if (node->prev != NULL) {
            // Check if the node is head
            node->prev->next = node->next; // if not disconnect previous node with current node
        } else {
            // if yes then mark next node as head
            list->head = node->next;
        }
        
        if (node->next != NULL) {
            // Check if the node is tail
            node->next->prev = node->prev; // if not disconnect next node with current node
        }
        
        // Delete node using free
        free(node);
        
        // Decrement list count
        list->count--;
    }
}

// Executes a function for each element in the list
void list_foreach(list_t* list, void (*func)(void* data))
{
    if (list != NULL && func != NULL) {
    	// Check if all arguments are valid
    	list_node_t* temp_node = list->head; // if yes create temp_node as head
    	
    	while (temp_node != NULL) {
    	    // Loop through list from head to tail
    	    func(temp_node->data); // Perform func on data
    	    temp_node = temp_node->next; // Go to next node
    	}
    }
}
