///// action.c

#include "actions.h"
#include "queue.h"
#include "send.h"
#include "server.h"
#include "stats.h"

#include <assert.h>
#include <evactions.h>
#include <stdlib.h>
#include <unistd.h>




//-----------------------------------------------------------------------------
// This action is created when we need to shutdown the server.  
void ah_server_shutdown(action_t *action)
{
	server_t *server;
	action_t *fire;
	stats_t *stats;
	system_data_t *sysdata;
	queue_t *q;
	node_t *node;
	int i;
	unsigned char verbose = 0;
	void *next;
	
	assert(action != NULL);
	assert(action->shared != NULL);
	sysdata = action->shared;
	
	assert(sysdata->server != NULL);
	server = sysdata->server;

	assert(sysdata->verbose >= 0);
	verbose = sysdata->verbose;

	if (server->shutdown == 0) {

		if (verbose) printf("Shutting down server object.\n");

		// close the listening handle and remove the listening event, we dont want
		// to accept any more connections.
		if (verbose) printf("Closing listening sockets.\n");
		for (i=0; i<MAX_SERVERS; i++) {
			if (server->servers[i].handle != INVALID_HANDLE) {
				if (verbose) printf("Closing socket %d.\n", server->servers[i].handle);

			  event_del(&server->servers[i].event);
				close(server->servers[i].handle);
				server->servers[i].handle = INVALID_HANDLE;
			}
		}

		// All active nodes should be informed that we are shutting down.
		assert(sysdata->actpool != NULL);
		if (verbose) printf("Firing event to shutdown nodes.\n");
		next = ll_start(&server->nodelist);
		node = ll_next(&server->nodelist, &next);
		while (node) {
			
			assert(node->handle > 0);

			// fire action_node_shutdown events for each node.
			fire = action_pool_new(sysdata->actpool);
			action_set(fire, 0, ah_node_shutdown, node);
			node->refcount ++;
			fire = NULL;

			if (verbose)
				printf("Initiating shutdown of node %d.\n", node->handle);
		
			node = ll_next(&server->nodelist, &next);
		}
		
		// fire an action for each queue.
		assert(sysdata->actpool != NULL);
		if (verbose) printf("Initiating shutdown of queues.\n");
		next = ll_start(sysdata->queues);
		q = ll_next(sysdata->queues, &next);
		while (q) {
			assert(q->name != NULL);
			assert(q->qid > 0);
	
			fire = action_pool_new(sysdata->actpool);
			action_set(fire, 250, ah_queue_shutdown, q);
			fire = NULL;
	
			if (verbose)
				printf("Initiating shutdown of queue %d ('%s').\n",
					q->qid,
					q->name);

			q = ll_next(sysdata->queues, &next);
		}

		// fire this action again, because we are not finished.
		server->shutdown = 1;
		action_timeout(action, 1000);
		action_reset(action);
	}
	else if (server->shutdown == 1) {
		// if already shutting down

		// check that all nodes have shutdown.
		if (ll_count(&server->nodelist) > 0 && verbose)
			printf("Waiting for %d nodes to shutdown.\n", server->active);
		else if (ll_count(sysdata->queues) > 0 && verbose)
			printf("Waiting for queues to shutdown.\n");
		
		if (ll_count(&server->nodelist) == 0 && ll_count(sysdata->queues) == 0) {
			// we are good to quit.
			// stop stats timer event.
			assert(sysdata != NULL);
			assert(sysdata->stats != NULL);
			stats = sysdata->stats;
			assert(stats->shutdown == 0);
			stats->shutdown ++;
			
			action_pool_return(action);
		}
		else {
// 			if (verbose)
// 				printf("Waiting for server to shutdown.\n");
			action_reset(action);
		}
	}
	else {
		// we are done, so we dont need to do anything more... the system should start shutting down now.
		assert(0);
	}
}


//-----------------------------------------------------------------------------
// shutdown the node.  If the node still has pending requests, then we need to
// wait until the requests have been returned or have timed out.
void ah_node_shutdown(action_t *action)
{
	system_data_t *sysdata;
	server_t *server;
	node_t *node;

	assert(action != NULL);
	assert(action->shared != NULL);
	assert(action->data != NULL);
	
	sysdata = action->shared;
	node = action->data;

	// if the node is consuming any queues, we need to cancel them (internally)
	if (BIT_TEST(node->flags, FLAG_NODE_NOQUEUES) == 0) {
		queue_cancel_node(node);
		BIT_SET(node->flags, FLAG_NODE_NOQUEUES);
	}

	// if the node is still connected, then we will attempt to first tell it that
	// we are closing the node.  If the node closed the connection first, then we
	// wont have a chance to do this.
	if (node->handle != INVALID_HANDLE && BIT_TEST(node->flags, FLAG_NODE_CLOSING) == 0) {
		sendClosing(node);
		BIT_SET(node->flags, FLAG_NODE_CLOSING);
	}

	// need to pay more attention to the lists here.  Something has closed, so one of the lists is no longer valid, need to determine what.
// 	assert(0);

	// We are still connected to the node, so we need to put a timeout on the
	// messages the node is servicing.
	if (node->handle != INVALID_HANDLE && (ll_count(&node->in_msg) > 0 || ll_count(&node->out_msg) > 0)) {

		// need to set a timeout on the messages.
		assert(0);
		
		// we still have stuff to do for this node, so we will reset the action to try again.
		action_timeout(action, 100);
		action_reset(action);
	}
	else {

		// we are not connected to the node anymore, so we cannot service the
		// messages that we were waiting for, so we need to return replies on
		// undelivery to the source nodes.
		if (node->handle == INVALID_HANDLE && (ll_count(&node->in_msg) > 0 || ll_count(&node->out_msg)>0)) {
			
			// send undelivered notices to the owners of all the pending messages
			// or return the message to the queue.
			assert(0);
		}

		// if we have done all we need to do, but still have a valid handle, then we should close it, and delete the event.
		if (node->handle != INVALID_HANDLE && node->out->length == 0) {
			assert(node->out->length == 0);
			assert(ll_count(&node->in_msg) == 0);
			assert(ll_count(&node->out_msg) == 0);
			assert(node->handle > 0);
			close(node->handle);
			node->handle = INVALID_HANDLE;

			assert(BIT_TEST(node->flags, FLAG_NODE_ACTIVE));
			assert(node->read_event);
			event_del(node->read_event);
			BIT_CLEAR(node->flags, FLAG_NODE_ACTIVE);

			node->refcount --;

			// remove the node from the server object.
			server = sysdata->server;
			assert(server);
			assert(ll_count(&server->nodelist) > 0);
			ll_remove(&server->nodelist, node, NULL);
			
			// free the resources used by the node.
			node_free(node);
			free(node);
	
			// we are done, so we dont need the action any more.
			action_pool_return(action);
		}
		else {
			// we have outgoing data that we are waiting to deliver... so continue to wait.
			action_timeout(action, 100);
			action_reset(action);
		}
	}
}



//-----------------------------------------------------------------------------
// shutdown the queue.  If there are messages in a queue waiting to be
// delivered or processed, we will need to wait for them.  Once all nodes have
// been stopped, then we can exit.
void ah_queue_shutdown(action_t *action)
{
	queue_t *queue;
	system_data_t *sysdata;
	
	assert(action != NULL);

	assert(action->shared != NULL);
	sysdata = action->shared;
	
	assert(action->data != NULL);
	queue = action->data;

	// if there is pending messages to deliver, then reply to the source, indicating unable to deliver.
	if (ll_count(&queue->msg_pending) > 0) {
		// We have a message that needs to be returned.

		assert(0);
	}
	
	// if we have messages being processed, might need to reply to source.
	if (ll_count(&queue->msg_proc) > 0) {
		// We have a message that needs to be returned.

		assert(0);
	}


	// delete the list of nodes that are consuming this queue.
	if (ll_count(&queue->nodes_busy) > 0) {
		assert(0);
	}

	if (ll_count(&queue->nodes_ready) > 0) {
		assert(0);
	}

	if (ll_count(&queue->nodes_waiting) > 0) {
		assert(0);
	}
	
	// Delete the queue from the queue list.
	ll_remove(sysdata->queues, queue, NULL);

	action_pool_return(action);
}



//-----------------------------------------------------------------------------
// This action handler handles the stats output.  Even if we are not doing any
// output this needs to run so that it can keep the counters under control.
void ah_stats(action_t *action)
{
	stats_t *stats;
	system_data_t *sysdata;

	assert(action && action->data && action->shared);
	stats = action->data;
	sysdata = action->shared;
	
	stats_display(stats, sysdata);

	if (stats->shutdown == 0) {
		// since we are not shutting down, schedule the action again.
		action_reset(action);
	}
	else {
		// we are not firing the action again, so we should return it to the pool it came from.
		action_pool_return(action);
	}
}


//-----------------------------------------------------------------------------
// This action is created when a message is provided that has a timeout.  This
// event will fire every second, decrementing the counter of the message.  If
// the countdown gets to 0, then it will initiate a 'return' of the failed
// message.

void ah_message(action_t *action)
{
	assert(action);
	assert(0);
}






