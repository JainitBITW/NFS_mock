#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_SIZE 10 // Define the size of the LRU cache

// A linked list node
typedef struct Node {
    char* key; // The key for the file name or path
    struct Node *prev, *next;
} Node;

// A queue (a list with eviction policy for the LRU cache)
typedef struct Queue {
    unsigned count; // Current size of the Queue
    unsigned capacity; // Capacity of the queue
    Node *front, *rear; // Front and rear of the queue
} Queue;

// A hash (to quickly find nodes in the queue by key)
typedef struct Hash {
    int capacity; // How many items can we store
    Node** array; // An array of pointers to nodes
} Hash;

// Create a new linked list node
Node* newNode(char* key) {
    Node* temp = (Node*)malloc(sizeof(Node));
    temp->key = strdup(key); // Allocate memory and copy the key
    temp->prev = temp->next = NULL;
    return temp;
}

// Create a queue of the given capacity
Queue* createQueue(int capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->count = 0;
    queue->front = queue->rear = NULL;
    queue->capacity = capacity;
    return queue;
}

// Create a hash of the given capacity
Hash* createHash(int capacity) {
    Hash* hash = (Hash*)malloc(sizeof(Hash));
    hash->capacity = capacity;
    hash->array = (Node**)malloc(hash->capacity * sizeof(Node*));
    int i;
    for (i = 0; i < hash->capacity; ++i)
        hash->array[i] = NULL;
    return hash;
}

// A utility function to check if there is slot available in the queue
int AreSlotsAvailable(Queue* queue) {
    return queue->count < queue->capacity;
}

// Function to remove a node from queue
void deQueue(Queue* queue) {
    if (AreSlotsAvailable(queue))
        return;

    // Remove the rear node
    if (queue->rear) {
        if (queue->rear->prev)
            queue->rear->prev->next = NULL;

        free(queue->rear->key);
        free(queue->rear);
    }
}

// Function to add a page with given 'key' to both queue and hash
void enqueue(Queue* queue, Hash* hash, char* key) {
    // If all slots are full, remove the least recently used (LRU) item
    if (AreSlotsAvailable(queue) == 0) {
        hash->array[queue->rear->key] = NULL;
        deQueue(queue);
    }

    // Create a new node with given key and add the front of queue
    Node* temp = newNode(key);
    temp->next = queue->front;

    // If queue is empty, change both front and rear pointers
    if (AreSlotsAvailable(queue)) {
        queue->rear = queue->front = temp;
    } else { // Else change the front
        queue->front->prev = temp;
        queue->front = temp;
    }

    // Add page entry to hash also
    hash->array[key] = temp;

    // increment number of full frames
    queue->count++;
}

// Function to bring the searched item to the front of the queue
void bringToQueueFront(Queue* queue, Hash* hash, char* key) {
    Node* reqPage = hash->array[key];
    if (reqPage == queue->front)
        return;

    // Unlink the node from its current location in the queue
    reqPage->prev->next = reqPage->next;
    if (reqPage->next)
       reqPage->next->prev = reqPage->prev;

    // If the node is the rear node, change the rear pointer
    if (reqPage == queue->rear) {
       queue->rear = reqPage->prev;
       queue->rear->next = NULL;
    }

    // Put the node in front
    reqPage->next = queue->front;
    reqPage->prev = NULL;

    // Change prev of current front
    reqPage->next->prev = reqPage;

    // Change front to the requested page
    queue->front = reqPage;
}

// Function to check if the given key is present in the hash
int searchInHash(Hash* hash, char* key) {
    if (hash->array[key] != NULL)
        return 1;
    return 0;
}

// Function to perform an efficient search based on the LRU cache
void performEfficientSearch(Queue* queue, Hash* hash, char* key) {
    if (!searchInHash(hash, key)) {
        enqueue(queue, hash, key);
    } else {
        bringToQueueFront(queue, hash, key);
    }
}

// Function to implement LRU cache
void implementLRUCache(char* key) {
    static Queue* q = NULL;
    static Hash* h = NULL;

    if (q == NULL || h == NULL) {
        q = createQueue(CACHE_SIZE);
        h = createHash(CACHE_SIZE);
    }

    performEfficientSearch(q, h, key);
}

int main() {
    // Simulate search requests
    implementLRUCache("file1.txt");
    implementLRUCache("file2.txt");
    // ... add more as needed

    return 0;
}
