#ifndef LINKED_LIST_H
#define LINKED_LIST_H
	#include <linux/types.h>
	struct Node {
		void* key;
		void* value;
		struct Node* next;
	};

	struct Node* createLinkedList(void);
	void insertNode(struct Node* head, void* key, void* value);
	void* findValue(struct Node* head, void* key);
	void cleanupLinkedList(struct Node* head,bool freeValue);
	bool removeValue(struct Node* head, void* key,bool freeValue);
	int countLinkedList(struct Node* head);
#endif /* LINKED_LIST_H */