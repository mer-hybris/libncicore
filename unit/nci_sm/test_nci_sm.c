/*
 * Copyright (C) 2019 Jolla Ltd.
 * Copyright (C) 2019 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "test_common.h"

#include "nci_types_p.h"
#include "nci_sm.h"
#include "nci_state.h"
#include "nci_state_p.h"
#include "nci_state_impl.h"
#include "nci_transition_impl.h"
#include "nci_param_w4_all_discoveries.h"

static TestOpt test_opt;

static
void
test_count_cb(
    NciSm* sm,
    void* user_data)
{
    (*((int*)user_data))++;
}

static
void
test_unreached_cb(
    NciSm* sm,
    void* user_data)
{
    g_assert(FALSE);
}

/*==========================================================================*
 * Test state
 *==========================================================================*/

typedef NciStateClass TestStateClass;
typedef struct test_state {
    NciState state;
    int entered;
    int reentered;
    int left;
} TestState;

G_DEFINE_TYPE(TestState, test_state, NCI_TYPE_STATE)
#define TEST_STATE_OBJ(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        TEST_TYPE_STATE, TestState))
#define TEST_TYPE_STATE (test_state_get_type())
#define TEST_STATE_PARENT (test_state_parent_class)
#define TEST_STATE NCI_CORE_STATES
#define INVALID_STATE (TEST_STATE + 1)

static
void
test_state_enter(
    NciState* state,
    NciParam* param)
{
    TEST_STATE_OBJ(state)->entered++;
    NCI_STATE_CLASS(test_state_parent_class)->enter(state, param);
}

static
void
test_state_reenter(
    NciState* state,
    NciParam* param)
{
    TEST_STATE_OBJ(state)->reentered++;
    NCI_STATE_CLASS(test_state_parent_class)->reenter(state, param);
}

static
void
test_state_leave(
    NciState* state)
{
    TEST_STATE_OBJ(state)->left++;
    NCI_STATE_CLASS(test_state_parent_class)->leave(state);
}

static
void
test_state_init(
    TestState* self)
{
}

static
void
test_state_class_init(
    TestStateClass* klass)
{
    klass->enter = test_state_enter;
    klass->reenter = test_state_reenter;
    klass->leave = test_state_leave;
}

TestState*
test_state_new(
    NciSm* sm,
    NCI_STATE state,
    const char* name)
{
    TestState* test = g_object_new(TEST_TYPE_STATE, NULL);

    nci_state_init_base(NCI_STATE(test), sm, state, name);
    return test;
}

/*==========================================================================*
 * Test transition
 *==========================================================================*/

typedef NciTransitionClass TestTransitionClass;
typedef struct test_transition {
    NciTransition transition;
    gboolean fail_start;
    int started;
    int finished;
} TestTransition;

G_DEFINE_TYPE(TestTransition, test_transition, NCI_TYPE_TRANSITION)
#define TEST_TYPE_TRANSITION (test_transition_get_type())
#define TEST_TRANSITION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        TEST_TYPE_TRANSITION, TestTransition))

static
gboolean
test_transition_start(
    NciTransition* transition)
{
    TestTransition* self = TEST_TRANSITION(transition);

    if (self->fail_start) {
        return NCI_TRANSITION_CLASS(test_transition_parent_class)->
            start(transition);
    } else {
        self->started++;
        nci_transition_finish(transition, NULL);
        return TRUE;
    }
}

static
void
test_transition_finished(
    NciTransition* transition)
{
    TestTransition* self = TEST_TRANSITION(transition);

    self->finished++;
    NCI_TRANSITION_CLASS(test_transition_parent_class)->finished(transition);
}

static
void
test_transition_init(
    TestTransition* self)
{
}

static
void
test_transition_class_init(
    TestTransitionClass* klass)
{
    klass->start = test_transition_start;
    klass->finished = test_transition_finished;
}

TestTransition*
test_transition_new(
    NciSm* sm,
    NCI_STATE state)
{
    NciState* dest = nci_sm_get_state(sm, state);

    if (dest) {
        TestTransition* self = g_object_new(TEST_TYPE_TRANSITION, NULL);

        nci_transition_init_base(&self->transition, sm, dest);
        return self;
    }
    return NULL;
}

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    NciSm* null = NULL;
    gulong zero = 0;

    g_assert(!nci_param_w4_all_discoveries_new(NULL));

    nci_sm_free(null);
    nci_sm_handle_ntf(null, 0, 0, NULL);
    nci_sm_add_state(null, NULL);
    nci_sm_add_state(sm, NULL);
    nci_sm_add_transition(null, NCI_STATE_INIT, NULL);
    nci_sm_add_transition(sm, NCI_STATE_INIT, NULL);
    g_assert(!nci_sm_add_last_state_handler(null, NULL, NULL));
    g_assert(!nci_sm_add_last_state_handler(sm, NULL, NULL));
    g_assert(!nci_sm_add_next_state_handler(null, NULL, NULL));
    g_assert(!nci_sm_add_next_state_handler(sm, NULL, NULL));
    g_assert(!nci_sm_add_intf_activated_handler(null, NULL, NULL));
    g_assert(!nci_sm_add_intf_activated_handler(sm, NULL, NULL));
    nci_sm_remove_handler(null, 0);
    nci_sm_remove_handler(null, 1);
    nci_sm_remove_handlers(null, NULL, 0);
    nci_sm_remove_handlers(sm, &zero, 1);
    g_assert(!nci_sm_sar(null));
    g_assert(!nci_sm_sar(sm));
    g_assert(!nci_sm_active_transition(null, NULL));
    g_assert(!nci_sm_active_transition(sm, NULL));
    nci_sm_add_weak_pointer(NULL);
    nci_sm_add_weak_pointer(&null);
    nci_sm_remove_weak_pointer(NULL);
    nci_sm_remove_weak_pointer(&null);
    g_assert(!nci_sm_get_state(null, NCI_STATE_INIT));
    g_assert(!nci_sm_get_state(sm, INVALID_STATE));
    g_assert(!nci_sm_enter_state(null, INVALID_STATE, NULL));
    g_assert(!nci_sm_enter_state(sm, INVALID_STATE, NULL));
    g_assert(!nci_sm_send_command(null, 0, 0, NULL, NULL, NULL));
    g_assert(!nci_sm_send_command(sm, 0, 0, NULL, NULL, NULL));
    g_assert(!nci_sm_send_command_static(null, 0, 0, NULL, 0, NULL, NULL));
    g_assert(!nci_sm_send_command_static(sm, 0, 0, NULL, 0, NULL, NULL));
    nci_sm_intf_activated(null, NULL);
    nci_sm_stall(null, NCI_STALL_ERROR);
    nci_sm_switch_to(null, NCI_STATE_INIT);
    nci_sm_free(sm);
}

/*==========================================================================*
 * state
 *==========================================================================*/

static
void
test_state(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    NciState* state = NCI_STATE(test_state_new(sm, TEST_STATE, "TEST"));
    NciState* null = NULL;

    g_assert(!nci_state_sm(null));
    g_assert(!nci_state_ref(null));
    nci_state_unref(null);
    nci_state_add_transition(null, NULL);
    nci_state_add_transition(state, NULL);
    g_assert(!nci_state_get_transition(null, NCI_STATE_INIT));
    g_assert(!nci_state_send_command(null, 0, 0, NULL, NULL, NULL));
    nci_state_enter(null, NULL);
    nci_state_reenter(null, NULL);
    nci_state_leave(null);
    nci_state_handle_ntf(null, 0, 0, NULL);

    nci_state_unref(state);
    nci_sm_free(sm);
}

/*==========================================================================*
 * transition
 *==========================================================================*/

static
void
test_transition(
    void)
{
    NciTransition* null = NULL;

    g_assert(!nci_transition_sm(null));
    g_assert(!nci_transition_ref(null));
    nci_transition_unref(null);
    nci_transition_stall(null, NCI_STALL_ERROR);
    g_assert(!nci_transition_active(null));
    g_assert(!nci_transition_start(null));
    nci_transition_finish(null, NULL);
    nci_transition_finished(null);
    nci_transition_handle_ntf(null, 0, 0, NULL);
    g_assert(!nci_transition_send_command(null, 0, 0, NULL, NULL));
    g_assert(!nci_transition_send_command_static(null, 0, 0, NULL, 0, NULL));
}

/*==========================================================================*
 * weak_ptr
 *==========================================================================*/

static
void
test_weak_ptr(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    NciSm* ptr1 = sm;
    NciSm* ptr2 = sm;

    nci_sm_add_weak_pointer(&ptr1);
    nci_sm_add_weak_pointer(&ptr2);
    nci_sm_remove_weak_pointer(&ptr1);
    g_assert(!ptr1);
    ptr1 = sm; /* It's no longer a weak pointer, it's just a pointer */

    nci_sm_free(sm);
    g_assert(ptr1);
    g_assert(!ptr2);
}

/*==========================================================================*
 * add_state
 *==========================================================================*/

static
void
test_add_state_cb(
    NciSm* sm,
    void* user_data)
{
    int* count = user_data;

    g_assert(sm->last_state->state == TEST_STATE);
    (*count)++;
}

static
void
test_add_state(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    TestState* test = test_state_new(sm, TEST_STATE, "TEST");
    NciState* state = NCI_STATE(test);
    int count = 0;
    gulong id = nci_sm_add_last_state_handler(sm, test_add_state_cb, &count);

    /* Second time doesn't do anything */
    nci_sm_add_state(sm, state);
    nci_sm_add_state(sm, state);

    g_assert(nci_sm_get_state(sm, TEST_STATE) == state);
    g_assert(nci_state_sm(state) == sm);
    g_assert(!count);

    /* Switch to our test state */
    g_assert(nci_sm_enter_state(sm, TEST_STATE, NULL));
    g_assert(test->entered == 1);
    g_assert(!test->reentered);
    g_assert(state->active);
    g_assert(count == 1);

    g_assert(nci_sm_enter_state(sm, TEST_STATE, NULL));
    g_assert(test->entered == 1);
    g_assert(test->reentered == 1);
    g_assert(state->active);
    g_assert(count == 1);

    nci_sm_remove_handlers(sm, &id, 1);
    g_assert(!id);
    
    nci_sm_free(sm);
    g_assert(!state->active);
    g_assert(test->left == 1);

    /* State is no longer associated with the state machine */
    g_assert(!nci_state_sm(state));
    nci_state_unref(state);
}

/*==========================================================================*
 * last_state
 *==========================================================================*/

static
void
test_last_state_cb(
    NciSm* sm,
    void* user_data)
{
    g_assert(sm == user_data);
    g_assert(sm->last_state->state == NCI_STATE_STOP);
    nci_sm_free(sm);
}

static
void
test_last_state(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    gulong id = nci_sm_add_last_state_handler(sm, test_unreached_cb, sm);

    /* Remove the dummy handler and add the real one */
    g_assert(id);
    nci_sm_remove_handler(sm, id);
    g_assert(nci_sm_add_last_state_handler(sm, test_last_state_cb, sm));
    nci_sm_add_weak_pointer(&sm);

    /* Switching to the same state has no effect */
    g_assert(sm->last_state->state == NCI_STATE_INIT);
    g_assert(nci_sm_enter_state(sm, NCI_STATE_INIT, NULL));
    g_assert(sm);

    /* This will switch state to NCI_STATE_STOP, handler will delete NciSm
     * and that will nullify the pointer */
    nci_sm_stall(sm, NCI_STALL_STOP);
    g_assert(!sm);
}

/*==========================================================================*
 * next_state
 *==========================================================================*/

static
void
test_next_state_cb(
    NciSm* sm,
    void* user_data)
{
    g_assert(sm == user_data);
    g_assert(sm->next_state->state == NCI_STATE_STOP);
    nci_sm_free(sm);
}

static
void
test_next_state(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    gulong id = nci_sm_add_next_state_handler(sm, test_unreached_cb, sm);

    /* Remove the dummy handler and add the real one */
    g_assert(id);
    nci_sm_remove_handler(sm, id);
    g_assert(nci_sm_add_next_state_handler(sm, test_next_state_cb, sm));
    nci_sm_add_weak_pointer(&sm);

    /* Switching to the same state has no effect */
    g_assert(sm->next_state->state == NCI_STATE_INIT);
    g_assert(nci_sm_enter_state(sm, NCI_STATE_INIT, NULL));
    g_assert(sm);

    /* This will switch state to NCI_STATE_STOP, handler will delete NciSm
     * and that will nullify the pointer */
    nci_sm_stall(sm, NCI_STALL_STOP);
    g_assert(!sm);
}

/*==========================================================================*
 * transitions
 *==========================================================================*/

static
void
test_transitions(
    void)
{
    NciSm* sm = nci_sm_new(NULL);
    TestState* test_state = test_state_new(sm, TEST_STATE, "TEST");
    TestTransition* test_transition;
    int count = 0;
    gulong id = nci_sm_add_next_state_handler(sm, test_count_cb, &count);

    g_assert(nci_sm_get_state(sm, NCI_RFST_IDLE));
    nci_sm_add_state(sm, NCI_STATE(test_state));
    test_transition = test_transition_new(sm, TEST_STATE);
    g_assert(test_transition);
    nci_sm_add_transition(sm, NCI_RFST_IDLE, &test_transition->transition);

    /* Simulate IDLE -> TEST switch failure */
    test_transition->fail_start = TRUE;
    g_assert(sm->last_state->state == NCI_STATE_INIT);
    g_assert(nci_sm_enter_state(sm, NCI_RFST_IDLE, NULL));
    g_assert(sm->last_state->state == NCI_RFST_IDLE);
    g_assert(count == 1);
    count = 0;

    nci_sm_switch_to(sm, TEST_STATE);
    g_assert(sm->last_state->state == NCI_STATE_ERROR);
    g_assert(count == 1);
    count = 0;

    /* And then a success */
    test_transition->fail_start = FALSE;
    nci_sm_switch_to(sm, NCI_STATE_INIT);
    g_assert(sm->last_state->state == NCI_STATE_INIT);
    g_assert(count == 1);
    count = 0;

    g_assert(nci_sm_enter_state(sm, NCI_RFST_IDLE, NULL));
    g_assert(sm->last_state->state == NCI_RFST_IDLE);
    g_assert(count == 1);
    count = 0;

    g_assert(!test_transition->finished);
    g_assert(!test_transition->started);
    nci_sm_switch_to(sm, TEST_STATE);
    g_assert(sm->last_state->state == TEST_STATE);
    g_assert(test_transition->started == 1);
    g_assert(test_transition->finished == 1);
    g_assert(count == 1);
    count = 0;

    nci_sm_remove_handler(sm, id);
    nci_sm_stall(sm, NCI_STALL_STOP);
    nci_sm_free(sm);

    nci_state_unref(NCI_STATE(test_state));
    nci_transition_unref(NCI_TRANSITION(test_transition));
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/nci_sm/" name

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("state"), test_state);
    g_test_add_func(TEST_("transition"), test_transition);
    g_test_add_func(TEST_("weak_ptr"), test_weak_ptr);
    g_test_add_func(TEST_("add_state"), test_add_state);
    g_test_add_func(TEST_("last_state"), test_last_state);
    g_test_add_func(TEST_("next_state"), test_next_state);
    g_test_add_func(TEST_("transitions"), test_transitions);
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
