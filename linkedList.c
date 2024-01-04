#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "linkedList.h"

struct Node* createLinkedList(void) {
    struct Node* head = kmalloc(sizeof(struct Node), GFP_KERNEL);
    if (head != NULL) {
        head->next = NULL;
    }
    return head;
}

bool removeValue(struct Node* head, void* key,bool freeValue) {
    struct Node* cur = head;
    struct Node* prev = NULL;
    while (cur != NULL) {
        if (cur->key == key) {
            if (prev != NULL) {
                prev->next = cur->next;
            } else {
                head = cur->next;
            }
			if(freeValue){
				kfree(cur->value);
			}
            kfree(cur);
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false; // Key not found
}

void insertNode(struct Node* head, void* key, void* value) {
    struct Node* newNode = kmalloc(sizeof(struct Node), GFP_KERNEL);
    if (newNode != NULL) {
        newNode->key = key;
        newNode->value = value;
        newNode->next = head->next;
        head->next = newNode;
    }
}

void* findValue(struct Node* head, void* key) {
    struct Node* t = head->next;
    while (t != NULL) {
        if (t->key == key) {
            return t->value;
        }
        t = t->next;
    }
    return NULL; // Key not found
}

void cleanupLinkedList(struct Node* head,bool freeValue) {
    struct Node* t = head->next;
    struct Node* temp;
    while (t != NULL) {
        temp = t;
        t = t->next;
		if(freeValue){
			kfree(temp->value);
		}
        kfree(temp);
    }
    kfree(head);
}

int countLinkedList(struct Node* head) {
    struct Node* t = head->next;
	int count=0;
    while (t != NULL) {
		count ++;
		t = t->next;
    }
	return count;
}