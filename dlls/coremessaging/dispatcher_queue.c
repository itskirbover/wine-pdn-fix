/* WinRT Windows.System.DispatcherQueue Implementation
 *
 * Copyright (C) 2025 Paint.NET Wine compatibility contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "private.h"

WINE_DEFAULT_DEBUG_CHANNEL(messaging);

/***********************************************************************
 *          IDispatcherQueueTimer
 */
struct dispatcher_queue_timer
{
    IDispatcherQueueTimer IDispatcherQueueTimer_iface;
    LONG ref;

    TimeSpan interval;
    BOOL is_running;
    BOOL is_repeating;
    INT64 next_token;
};

static inline struct dispatcher_queue_timer *impl_from_IDispatcherQueueTimer( IDispatcherQueueTimer *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_timer, IDispatcherQueueTimer_iface );
}

static HRESULT WINAPI dispatcher_queue_timer_QueryInterface( IDispatcherQueueTimer *iface, REFIID iid, void **out )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IDispatcherQueueTimer ))
    {
        *out = &impl->IDispatcherQueueTimer_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI dispatcher_queue_timer_AddRef( IDispatcherQueueTimer *iface )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing ref to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI dispatcher_queue_timer_Release( IDispatcherQueueTimer *iface )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing ref to %lu.\n", iface, ref );
    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI dispatcher_queue_timer_GetIids( IDispatcherQueueTimer *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_timer_GetRuntimeClassName( IDispatcherQueueTimer *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    if (!class_name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_System_DispatcherQueueTimer,
                                wcslen( RuntimeClass_Windows_System_DispatcherQueueTimer ), class_name );
}

static HRESULT WINAPI dispatcher_queue_timer_GetTrustLevel( IDispatcherQueueTimer *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_get_Interval( IDispatcherQueueTimer *iface, TimeSpan *value )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->interval;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_put_Interval( IDispatcherQueueTimer *iface, TimeSpan value )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    TRACE( "iface %p, value %I64d.\n", iface, value.Duration );
    impl->interval = value;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_get_IsRunning( IDispatcherQueueTimer *iface, boolean *value )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->is_running;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_get_IsRepeating( IDispatcherQueueTimer *iface, boolean *value )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    if (!value) return E_POINTER;
    *value = impl->is_repeating;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_put_IsRepeating( IDispatcherQueueTimer *iface, boolean value )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    TRACE( "iface %p, value %u.\n", iface, value );
    impl->is_repeating = !!value;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_Start( IDispatcherQueueTimer *iface )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    FIXME( "iface %p stub!\n", iface );
    impl->is_running = TRUE;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_Stop( IDispatcherQueueTimer *iface )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    FIXME( "iface %p stub!\n", iface );
    impl->is_running = FALSE;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_add_Tick( IDispatcherQueueTimer *iface,
        ITypedEventHandler_DispatcherQueueTimer_IInspectable *handler, EventRegistrationToken *token )
{
    struct dispatcher_queue_timer *impl = impl_from_IDispatcherQueueTimer( iface );
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    if (!handler || !token) return E_POINTER;
    token->value = ++impl->next_token;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_timer_remove_Tick( IDispatcherQueueTimer *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64d stub!\n", iface, token.value );
    return S_OK;
}

static const struct IDispatcherQueueTimerVtbl dispatcher_queue_timer_vtbl =
{
    /* IUnknown methods */
    dispatcher_queue_timer_QueryInterface,
    dispatcher_queue_timer_AddRef,
    dispatcher_queue_timer_Release,
    /* IInspectable methods */
    dispatcher_queue_timer_GetIids,
    dispatcher_queue_timer_GetRuntimeClassName,
    dispatcher_queue_timer_GetTrustLevel,
    /* IDispatcherQueueTimer methods */
    dispatcher_queue_timer_get_Interval,
    dispatcher_queue_timer_put_Interval,
    dispatcher_queue_timer_get_IsRunning,
    dispatcher_queue_timer_get_IsRepeating,
    dispatcher_queue_timer_put_IsRepeating,
    dispatcher_queue_timer_Start,
    dispatcher_queue_timer_Stop,
    dispatcher_queue_timer_add_Tick,
    dispatcher_queue_timer_remove_Tick,
};

static HRESULT dispatcher_queue_timer_create( IDispatcherQueueTimer **out )
{
    struct dispatcher_queue_timer *impl;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->IDispatcherQueueTimer_iface.lpVtbl = &dispatcher_queue_timer_vtbl;
    impl->ref = 1;
    impl->is_repeating = TRUE;

    *out = &impl->IDispatcherQueueTimer_iface;
    return S_OK;
}

/***********************************************************************
 *          IDispatcherQueue / IDispatcherQueue2
 */
struct dispatcher_queue
{
    IDispatcherQueue IDispatcherQueue_iface;
    IDispatcherQueue2 IDispatcherQueue2_iface;
    LONG ref;

    DWORD thread_id;
    INT64 next_token;
};

static inline struct dispatcher_queue *impl_from_IDispatcherQueue( IDispatcherQueue *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue, IDispatcherQueue_iface );
}

static HRESULT WINAPI dispatcher_queue_QueryInterface( IDispatcherQueue *iface, REFIID iid, void **out )
{
    struct dispatcher_queue *impl = impl_from_IDispatcherQueue( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IDispatcherQueue ))
    {
        *out = &impl->IDispatcherQueue_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IDispatcherQueue2 ))
    {
        *out = &impl->IDispatcherQueue2_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI dispatcher_queue_AddRef( IDispatcherQueue *iface )
{
    struct dispatcher_queue *impl = impl_from_IDispatcherQueue( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing ref to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI dispatcher_queue_Release( IDispatcherQueue *iface )
{
    struct dispatcher_queue *impl = impl_from_IDispatcherQueue( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing ref to %lu.\n", iface, ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI dispatcher_queue_GetIids( IDispatcherQueue *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_GetRuntimeClassName( IDispatcherQueue *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    if (!class_name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_System_DispatcherQueue,
                                wcslen( RuntimeClass_Windows_System_DispatcherQueue ), class_name );
}

static HRESULT WINAPI dispatcher_queue_GetTrustLevel( IDispatcherQueue *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_CreateTimer( IDispatcherQueue *iface, IDispatcherQueueTimer **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    if (!result) return E_POINTER;
    return dispatcher_queue_timer_create( result );
}

static HRESULT WINAPI dispatcher_queue_TryEnqueue( IDispatcherQueue *iface, IDispatcherQueueHandler *callback, boolean *result )
{
    FIXME( "iface %p, callback %p, result %p stub!\n", iface, callback, result );
    if (!result) return E_POINTER;
    if (!callback)
    {
        *result = FALSE;
        return E_INVALIDARG;
    }
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_TryEnqueueWithPriority( IDispatcherQueue *iface, DispatcherQueuePriority priority,
        IDispatcherQueueHandler *callback, boolean *result )
{
    FIXME( "iface %p, priority %d, callback %p, result %p stub!\n", iface, priority, callback, result );
    if (!result) return E_POINTER;
    if (!callback)
    {
        *result = FALSE;
        return E_INVALIDARG;
    }
    *result = TRUE;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_add_ShutdownStarting( IDispatcherQueue *iface,
        ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs *handler,
        EventRegistrationToken *token )
{
    struct dispatcher_queue *impl = impl_from_IDispatcherQueue( iface );
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    if (!handler || !token) return E_POINTER;
    token->value = ++impl->next_token;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_remove_ShutdownStarting( IDispatcherQueue *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64d stub!\n", iface, token.value );
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_add_ShutdownCompleted( IDispatcherQueue *iface,
        ITypedEventHandler_DispatcherQueue_IInspectable *handler, EventRegistrationToken *token )
{
    struct dispatcher_queue *impl = impl_from_IDispatcherQueue( iface );
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    if (!handler || !token) return E_POINTER;
    token->value = ++impl->next_token;
    return S_OK;
}

static HRESULT WINAPI dispatcher_queue_remove_ShutdownCompleted( IDispatcherQueue *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64d stub!\n", iface, token.value );
    return S_OK;
}

static const struct IDispatcherQueueVtbl dispatcher_queue_vtbl =
{
    /* IUnknown methods */
    dispatcher_queue_QueryInterface,
    dispatcher_queue_AddRef,
    dispatcher_queue_Release,
    /* IInspectable methods */
    dispatcher_queue_GetIids,
    dispatcher_queue_GetRuntimeClassName,
    dispatcher_queue_GetTrustLevel,
    /* IDispatcherQueue methods */
    dispatcher_queue_CreateTimer,
    dispatcher_queue_TryEnqueue,
    dispatcher_queue_TryEnqueueWithPriority,
    dispatcher_queue_add_ShutdownStarting,
    dispatcher_queue_remove_ShutdownStarting,
    dispatcher_queue_add_ShutdownCompleted,
    dispatcher_queue_remove_ShutdownCompleted,
};

DEFINE_IINSPECTABLE( dispatcher_queue2, IDispatcherQueue2, struct dispatcher_queue, IDispatcherQueue_iface )

static HRESULT WINAPI dispatcher_queue2_get_HasThreadAccess( IDispatcherQueue2 *iface, boolean *value )
{
    struct dispatcher_queue *impl = impl_from_IDispatcherQueue2( iface );

    TRACE( "iface %p, value %p.\n", iface, value );

    if (!value) return E_POINTER;
    *value = (GetCurrentThreadId() == impl->thread_id);
    return S_OK;
}

static const struct IDispatcherQueue2Vtbl dispatcher_queue2_vtbl =
{
    /* IUnknown methods */
    dispatcher_queue2_QueryInterface,
    dispatcher_queue2_AddRef,
    dispatcher_queue2_Release,
    /* IInspectable methods */
    dispatcher_queue2_GetIids,
    dispatcher_queue2_GetRuntimeClassName,
    dispatcher_queue2_GetTrustLevel,
    /* IDispatcherQueue2 methods */
    dispatcher_queue2_get_HasThreadAccess,
};

static HRESULT dispatcher_queue_create( IDispatcherQueue **out )
{
    struct dispatcher_queue *impl;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->IDispatcherQueue_iface.lpVtbl = &dispatcher_queue_vtbl;
    impl->IDispatcherQueue2_iface.lpVtbl = &dispatcher_queue2_vtbl;
    impl->ref = 1;
    impl->thread_id = GetCurrentThreadId();

    *out = &impl->IDispatcherQueue_iface;
    return S_OK;
}

/***********************************************************************
 *          IDispatcherQueueStatics
 */
struct dispatcher_queue_statics
{
    IActivationFactory IActivationFactory_iface;
    IDispatcherQueueStatics IDispatcherQueueStatics_iface;
    LONG ref;
};

static inline struct dispatcher_queue_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_statics, IActivationFactory_iface );
}

static HRESULT WINAPI dispatcher_queue_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct dispatcher_queue_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IDispatcherQueueStatics ))
    {
        *out = &impl->IDispatcherQueueStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI dispatcher_queue_factory_AddRef( IActivationFactory *iface )
{
    struct dispatcher_queue_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing ref to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI dispatcher_queue_factory_Release( IActivationFactory *iface )
{
    struct dispatcher_queue_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing ref to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI dispatcher_queue_factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI dispatcher_queue_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    /* IUnknown methods */
    dispatcher_queue_factory_QueryInterface,
    dispatcher_queue_factory_AddRef,
    dispatcher_queue_factory_Release,
    /* IInspectable methods */
    dispatcher_queue_factory_GetIids,
    dispatcher_queue_factory_GetRuntimeClassName,
    dispatcher_queue_factory_GetTrustLevel,
    /* IActivationFactory methods */
    dispatcher_queue_factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( dispatcher_queue_statics, IDispatcherQueueStatics, struct dispatcher_queue_statics, IActivationFactory_iface )

static HRESULT WINAPI dispatcher_queue_statics_GetForCurrentThread( IDispatcherQueueStatics *iface, IDispatcherQueue **out )
{
    FIXME( "iface %p, out %p semi-stub!\n", iface, out );
    if (!out) return E_POINTER;
    return dispatcher_queue_create( out );
}

static const struct IDispatcherQueueStaticsVtbl dispatcher_queue_statics_vtbl =
{
    dispatcher_queue_statics_QueryInterface,
    dispatcher_queue_statics_AddRef,
    dispatcher_queue_statics_Release,
    /* IInspectable methods */
    dispatcher_queue_statics_GetIids,
    dispatcher_queue_statics_GetRuntimeClassName,
    dispatcher_queue_statics_GetTrustLevel,
    /* IDispatcherQueueStatics methods */
    dispatcher_queue_statics_GetForCurrentThread,
};

static struct dispatcher_queue_statics dispatcher_queue_statics =
{
    {&factory_vtbl},
    {&dispatcher_queue_statics_vtbl},
    1,
};

IActivationFactory *dispatcher_queue_factory = &dispatcher_queue_statics.IActivationFactory_iface;
