/*
 * Uianimation main file.
 *
 * Copyright (C) 2018 Louis Lenders
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
#define COBJMACROS

#include <math.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "rpcproxy.h"
#include "oaidl.h"
#include "ocidl.h"

#include "initguid.h"
#include "uianimation.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(uianimation);

struct class_factory
{
    IClassFactory IClassFactory_iface;
    HRESULT (*create_instance)(IUnknown *, REFIID, void **);
};

static inline struct class_factory *impl_from_IClassFactory( IClassFactory *iface )
{
    return CONTAINING_RECORD( iface, struct class_factory, IClassFactory_iface );
}

static HRESULT WINAPI class_factory_QueryInterface( IClassFactory *iface, REFIID iid, void **obj )
{
    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IClassFactory ))
    {
        IClassFactory_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef( IClassFactory *iface )
{
    return 2;
}

static ULONG WINAPI class_factory_Release( IClassFactory *iface )
{
    return 1;
}

static HRESULT WINAPI class_factory_CreateInstance( IClassFactory *iface,
                                                    IUnknown *outer, REFIID iid,
                                                    void **obj )
{
    struct class_factory *This = impl_from_IClassFactory( iface );

    TRACE( "%p %s %p\n", outer, debugstr_guid( iid ), obj );

    *obj = NULL;
    return This->create_instance( outer, iid, obj );
}

static HRESULT WINAPI class_factory_LockServer( IClassFactory *iface,
                                                BOOL lock )
{
    FIXME( "%d: stub!\n", lock );
    return S_OK;
}

static const struct IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer
};

struct transition
{
    IUIAnimationTransition IUIAnimationTransition_iface;
    LONG ref;

    BOOL has_final_value;
    DOUBLE final_value;
    BOOL has_duration;
    UI_ANIMATION_SECONDS duration;
    DOUBLE initial_value;
    DOUBLE initial_velocity;
};

struct animation_var;
static struct transition *unsafe_impl_from_IUIAnimationTransition( IUIAnimationTransition *iface );
static struct animation_var *unsafe_impl_from_IUIAnimationVariable( IUIAnimationVariable *iface );
static HRESULT animation_var_set_value( struct animation_var *var, IUIAnimationStoryboard *storyboard, DOUBLE value );

/***********************************************************************
 *          IUIAnimationStoryboard
 */
struct storyboard_entry
{
    IUIAnimationVariable *variable;
    IUIAnimationTransition *transition;
};

struct animation_storyboard
{
    IUIAnimationStoryboard IUIAnimationStoryboard_iface;
    LONG ref;

    struct storyboard_entry *entries;
    size_t count;
    size_t capacity;

    ULONG_PTR next_keyframe;
    UI_ANIMATION_STORYBOARD_STATUS status;
    IUnknown *tag_object;
    UINT32 tag_id;
    BOOL tag_set;
    IUIAnimationStoryboardEventHandler *event_handler;
};

static inline struct animation_storyboard *impl_from_IUIAnimationStoryboard( IUIAnimationStoryboard *iface )
{
    return CONTAINING_RECORD( iface, struct animation_storyboard, IUIAnimationStoryboard_iface );
}

static HRESULT storyboard_add_entry( struct animation_storyboard *This, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition )
{
    if (!variable || !transition)
        return E_POINTER;

    if (This->count == This->capacity)
    {
        size_t new_capacity = This->capacity ? This->capacity * 2 : 8;
        struct storyboard_entry *new_entries = realloc( This->entries, new_capacity * sizeof(*new_entries) );
        if (!new_entries)
            return E_OUTOFMEMORY;
        This->entries = new_entries;
        This->capacity = new_capacity;
    }

    IUIAnimationVariable_AddRef( variable );
    IUIAnimationTransition_AddRef( transition );
    This->entries[This->count].variable = variable;
    This->entries[This->count].transition = transition;
    This->count++;
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_QueryInterface( IUIAnimationStoryboard *iface,
                                                 REFIID iid, void **obj )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationStoryboard ))
    {
        IUIAnimationStoryboard_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI animation_storyboard_AddRef( IUIAnimationStoryboard *iface )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI animation_storyboard_Release( IUIAnimationStoryboard *iface )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    ULONG ref = InterlockedDecrement( &This->ref );
    size_t i;

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
    {
        for (i = 0; i < This->count; i++)
        {
            IUIAnimationVariable_Release( This->entries[i].variable );
            IUIAnimationTransition_Release( This->entries[i].transition );
        }
        free( This->entries );
        if (This->tag_object)
            IUnknown_Release( This->tag_object );
        if (This->event_handler)
            IUIAnimationStoryboardEventHandler_Release( This->event_handler );
        free( This );
    }

    return ref;
}

static HRESULT WINAPI animation_storyboard_AddTransition( IUIAnimationStoryboard *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    TRACE( "(%p)->(%p, %p)\n", This, variable, transition );
    return storyboard_add_entry( This, variable, transition );
}

static HRESULT WINAPI animation_storyboard_AddKeyframeAtOffset( IUIAnimationStoryboard *iface, UI_ANIMATION_KEYFRAME existingframe,
        UI_ANIMATION_SECONDS offset, UI_ANIMATION_KEYFRAME *keyframe )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    FIXME( "stub (%p)->(%p, %f, %p)\n", This, existingframe, offset, keyframe );

    if (!keyframe)
        return E_POINTER;

    *keyframe = (UI_ANIMATION_KEYFRAME)(++This->next_keyframe);
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_AddKeyframeAfterTransition( IUIAnimationStoryboard *iface, IUIAnimationTransition *transition,
        UI_ANIMATION_KEYFRAME *keyframe )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    FIXME( "stub (%p)->(%p, %p)\n", This, transition, keyframe );

    if (!keyframe)
        return E_POINTER;

    *keyframe = (UI_ANIMATION_KEYFRAME)(++This->next_keyframe);
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_AddTransitionAtKeyframe( IUIAnimationStoryboard *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition, UI_ANIMATION_KEYFRAME start_key )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    FIXME( "partial stub (%p)->(%p, %p, %p)\n", This, variable, transition, start_key );
    return storyboard_add_entry( This, variable, transition );
}

static HRESULT WINAPI animation_storyboard_AddTransitionBetweenKeyframes( IUIAnimationStoryboard *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition, UI_ANIMATION_KEYFRAME start_key, UI_ANIMATION_KEYFRAME end_key )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    FIXME( "partial stub (%p)->(%p, %p, %p, %p)\n", This, variable, transition, start_key, end_key );
    return storyboard_add_entry( This, variable, transition );
}

static HRESULT WINAPI animation_storyboard_RepeatBetweenKeyframes( IUIAnimationStoryboard *iface, UI_ANIMATION_KEYFRAME start_key,
        UI_ANIMATION_KEYFRAME end_key, INT32 count )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->(%p, %p, %d)\n", This, start_key, end_key, count );
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_HoldVariable( IUIAnimationStoryboard *iface, IUIAnimationVariable *variable )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->(%p)\n", This, variable );
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_SetLongestAcceptableDelay( IUIAnimationStoryboard *iface, UI_ANIMATION_SECONDS delay )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->(%f)\n", This, delay );
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_Schedule( IUIAnimationStoryboard *iface, UI_ANIMATION_SECONDS now,
        UI_ANIMATION_SCHEDULING_RESULT *result )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    UI_ANIMATION_STORYBOARD_STATUS prev_status;
    size_t i;

    TRACE( "(%p)->(%f, %p)\n", This, now, result );

    IUIAnimationStoryboard_AddRef( iface );

    for (i = 0; i < This->count; i++)
    {
        struct transition *tr = unsafe_impl_from_IUIAnimationTransition( This->entries[i].transition );
        struct animation_var *var = unsafe_impl_from_IUIAnimationVariable( This->entries[i].variable );

        if (tr && var && tr->has_final_value)
            animation_var_set_value( var, iface, tr->final_value );
        else if (!tr || !var)
            FIXME( "unsupported transition %p or variable %p\n", This->entries[i].transition, This->entries[i].variable );
    }

    prev_status = This->status;
    This->status = UI_ANIMATION_STORYBOARD_FINISHED;
    if (This->event_handler)
    {
        IUIAnimationStoryboardEventHandler *handler = This->event_handler;
        IUIAnimationStoryboardEventHandler_AddRef( handler );
        IUIAnimationStoryboardEventHandler_OnStoryboardStatusChanged( handler, iface, This->status, prev_status );
        IUIAnimationStoryboardEventHandler_OnStoryboardUpdated( handler, iface );
        IUIAnimationStoryboardEventHandler_Release( handler );
    }

    if (result)
        *result = UI_ANIMATION_SCHEDULING_SUCCEEDED;

    IUIAnimationStoryboard_Release( iface );
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_Conclude( IUIAnimationStoryboard *iface )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->()\n", This );
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_Finish( IUIAnimationStoryboard *iface, UI_ANIMATION_SECONDS deadline )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->(%f)\n", This, deadline );
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_Abandon( IUIAnimationStoryboard *iface )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->()\n", This );
    This->status = UI_ANIMATION_STORYBOARD_CANCELLED;
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_SetTag( IUIAnimationStoryboard *iface, IUnknown *object, UINT32 id )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    TRACE( "(%p)->(%p, %u)\n", This, object, id );

    if (object) IUnknown_AddRef( object );
    if (This->tag_object) IUnknown_Release( This->tag_object );
    This->tag_object = object;
    This->tag_id = id;
    This->tag_set = TRUE;
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_GetTag( IUIAnimationStoryboard *iface, IUnknown **object, UINT32 *id )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    TRACE( "(%p)->(%p, %p)\n", This, object, id );

    if (!This->tag_set)
    {
        if (object) *object = NULL;
        return UI_E_VALUE_NOT_SET;
    }

    if (object)
    {
        *object = This->tag_object;
        if (*object) IUnknown_AddRef( *object );
    }
    if (id) *id = This->tag_id;
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_GetStatus( IUIAnimationStoryboard *iface, UI_ANIMATION_STORYBOARD_STATUS *status )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    TRACE( "(%p)->(%p)\n", This, status );

    if (!status)
        return E_POINTER;

    *status = This->status;
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_GetElapsedTime( IUIAnimationStoryboard *iface, UI_ANIMATION_SECONDS *elapsed )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );
    FIXME( "stub (%p)->(%p)\n", This, elapsed );
    if (!elapsed)
        return E_POINTER;
    *elapsed = 0.0;
    return S_OK;
}

static HRESULT WINAPI animation_storyboard_SetStoryboardEventHandler( IUIAnimationStoryboard *iface, IUIAnimationStoryboardEventHandler *handler )
{
    struct animation_storyboard *This = impl_from_IUIAnimationStoryboard( iface );

    TRACE( "(%p)->(%p)\n", This, handler );

    if (handler) IUIAnimationStoryboardEventHandler_AddRef( handler );
    if (This->event_handler) IUIAnimationStoryboardEventHandler_Release( This->event_handler );
    This->event_handler = handler;
    return S_OK;
}

static const struct IUIAnimationStoryboardVtbl animation_storyboard_vtbl =
{
    animation_storyboard_QueryInterface,
    animation_storyboard_AddRef,
    animation_storyboard_Release,
    animation_storyboard_AddTransition,
    animation_storyboard_AddKeyframeAtOffset,
    animation_storyboard_AddKeyframeAfterTransition,
    animation_storyboard_AddTransitionAtKeyframe,
    animation_storyboard_AddTransitionBetweenKeyframes,
    animation_storyboard_RepeatBetweenKeyframes,
    animation_storyboard_HoldVariable,
    animation_storyboard_SetLongestAcceptableDelay,
    animation_storyboard_Schedule,
    animation_storyboard_Conclude,
    animation_storyboard_Finish,
    animation_storyboard_Abandon,
    animation_storyboard_SetTag,
    animation_storyboard_GetTag,
    animation_storyboard_GetStatus,
    animation_storyboard_GetElapsedTime,
    animation_storyboard_SetStoryboardEventHandler
};

static HRESULT animation_storyboard_create( IUIAnimationStoryboard **obj )
{
    struct animation_storyboard *This = calloc( 1, sizeof(*This) );

    if (!This) return E_OUTOFMEMORY;
    This->IUIAnimationStoryboard_iface.lpVtbl = &animation_storyboard_vtbl;
    This->ref = 1;
    This->status = UI_ANIMATION_STORYBOARD_BUILDING;

    *obj = &This->IUIAnimationStoryboard_iface;

    return S_OK;
}

/***********************************************************************
 *          IUIAnimationVariable
 */
struct animation_var
{
    IUIAnimationVariable IUIAnimationVariable_iface;
    LONG ref;

    DOUBLE value;
    DOUBLE initial_value;
    DOUBLE previous_value;
    DOUBLE final_value;
    DOUBLE lower_bound;
    DOUBLE upper_bound;
    UI_ANIMATION_ROUNDING_MODE rounding_mode;

    IUnknown *tag_object;
    UINT32 tag_id;
    BOOL tag_set;

    IUIAnimationVariableChangeHandler *change_handler;
    IUIAnimationVariableIntegerChangeHandler *int_change_handler;
};

static inline struct animation_var *impl_from_IUIAnimationVariable( IUIAnimationVariable *iface )
{
    return CONTAINING_RECORD( iface, struct animation_var, IUIAnimationVariable_iface );
}

static INT32 animation_var_round( const struct animation_var *var, DOUBLE val )
{
    switch (var->rounding_mode)
    {
    case UI_ANIMATION_ROUNDING_FLOOR:
        return (INT32)floor( val );
    case UI_ANIMATION_ROUNDING_CEILING:
        return (INT32)ceil( val );
    case UI_ANIMATION_ROUNDING_NEAREST:
    default:
        return (INT32)round( val );
    }
}

static HRESULT WINAPI animation_var_QueryInterface( IUIAnimationVariable *iface,
                                                 REFIID iid, void **obj )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationVariable ))
    {
        IUIAnimationVariable_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI animation_var_AddRef( IUIAnimationVariable *iface )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI animation_var_Release( IUIAnimationVariable *iface )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
    {
        if (This->tag_object)
            IUnknown_Release( This->tag_object );
        if (This->change_handler)
            IUIAnimationVariableChangeHandler_Release( This->change_handler );
        if (This->int_change_handler)
            IUIAnimationVariableIntegerChangeHandler_Release( This->int_change_handler );
        free( This );
    }

    return ref;
}

static HRESULT WINAPI animation_var_GetValue( IUIAnimationVariable *iface, DOUBLE *value )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, value );

    if (!value)
        return E_POINTER;

    *value = This->value;
    return S_OK;
}

static HRESULT WINAPI animation_var_GetFinalValue( IUIAnimationVariable *iface, DOUBLE *value )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, value );

    if (!value)
        return E_POINTER;

    *value = This->final_value;
    return S_OK;
}

static HRESULT WINAPI animation_var_GetPreviousValue( IUIAnimationVariable *iface, DOUBLE *value )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, value );

    if (!value)
        return E_POINTER;

    *value = This->previous_value;
    return S_OK;
}

static HRESULT WINAPI animation_var_GetIntegerValue( IUIAnimationVariable *iface, INT32 *value )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, value );

    if (!value)
        return E_POINTER;

    *value = animation_var_round( This, This->value );
    return S_OK;
}

static HRESULT WINAPI animation_var_GetFinalIntegerValue( IUIAnimationVariable *iface, INT32 *value )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, value );

    if (!value)
        return E_POINTER;

    *value = animation_var_round( This, This->final_value );
    return S_OK;
}

static HRESULT WINAPI animation_var_GetPreviousIntegerValue( IUIAnimationVariable *iface, INT32 *value )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, value );

    if (!value)
        return E_POINTER;

    *value = animation_var_round( This, This->previous_value );
    return S_OK;
}

static HRESULT WINAPI animation_var_GetCurrentStoryboard( IUIAnimationVariable *iface, IUIAnimationStoryboard **storyboard )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, storyboard );

    if (!storyboard)
        return E_POINTER;

    *storyboard = NULL;
    return S_OK;
}

static HRESULT WINAPI animation_var_SetLowerBound( IUIAnimationVariable *iface, DOUBLE bound )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%f)\n", This, bound );

    This->lower_bound = bound;
    return S_OK;
}

static HRESULT WINAPI animation_var_SetUpperBound( IUIAnimationVariable *iface, DOUBLE bound )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%f)\n", This, bound );

    This->upper_bound = bound;
    return S_OK;
}

static HRESULT WINAPI animation_var_SetRoundingMode( IUIAnimationVariable *iface, UI_ANIMATION_ROUNDING_MODE mode )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%d)\n", This, mode );

    This->rounding_mode = mode;
    return S_OK;
}

static HRESULT WINAPI animation_var_SetTag( IUIAnimationVariable *iface, IUnknown *object, UINT32 id )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p, %u)\n", This, object, id );

    if (object) IUnknown_AddRef( object );
    if (This->tag_object) IUnknown_Release( This->tag_object );
    This->tag_object = object;
    This->tag_id = id;
    This->tag_set = TRUE;
    return S_OK;
}

static HRESULT WINAPI animation_var_GetTag( IUIAnimationVariable *iface, IUnknown **object, UINT32 *id )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p, %p)\n", This, object, id );

    if (!This->tag_set)
    {
        if (object) *object = NULL;
        return UI_E_VALUE_NOT_SET;
    }

    if (object)
    {
        *object = This->tag_object;
        if (*object) IUnknown_AddRef( *object );
    }
    if (id) *id = This->tag_id;
    return S_OK;
}

static HRESULT WINAPI animation_var_SetVariableChangeHandler( IUIAnimationVariable *iface, IUIAnimationVariableChangeHandler *handler )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, handler );

    if (handler) IUIAnimationVariableChangeHandler_AddRef( handler );
    if (This->change_handler) IUIAnimationVariableChangeHandler_Release( This->change_handler );
    This->change_handler = handler;
    return S_OK;
}

static HRESULT WINAPI animation_var_SetVariableIntegerChangeHandler( IUIAnimationVariable *iface,
        IUIAnimationVariableIntegerChangeHandler *handler )
{
    struct animation_var *This = impl_from_IUIAnimationVariable( iface );

    TRACE( "(%p)->(%p)\n", This, handler );

    if (handler) IUIAnimationVariableIntegerChangeHandler_AddRef( handler );
    if (This->int_change_handler) IUIAnimationVariableIntegerChangeHandler_Release( This->int_change_handler );
    This->int_change_handler = handler;
    return S_OK;
}

static const struct IUIAnimationVariableVtbl animation_var_vtbl =
{
    animation_var_QueryInterface,
    animation_var_AddRef,
    animation_var_Release,
    animation_var_GetValue,
    animation_var_GetFinalValue,
    animation_var_GetPreviousValue,
    animation_var_GetIntegerValue,
    animation_var_GetFinalIntegerValue,
    animation_var_GetPreviousIntegerValue,
    animation_var_GetCurrentStoryboard,
    animation_var_SetLowerBound,
    animation_var_SetUpperBound,
    animation_var_SetRoundingMode,
    animation_var_SetTag,
    animation_var_GetTag,
    animation_var_SetVariableChangeHandler,
    animation_var_SetVariableIntegerChangeHandler,
};

static struct animation_var *unsafe_impl_from_IUIAnimationVariable( IUIAnimationVariable *iface )
{
    if (!iface || iface->lpVtbl != &animation_var_vtbl)
        return NULL;
    return CONTAINING_RECORD( iface, struct animation_var, IUIAnimationVariable_iface );
}

static HRESULT animation_var_create( DOUBLE initial, IUIAnimationVariable **obj )
{
    struct animation_var *This = calloc( 1, sizeof(*This) );

    if (!This) return E_OUTOFMEMORY;
    This->IUIAnimationVariable_iface.lpVtbl = &animation_var_vtbl;
    This->ref = 1;
    This->initial_value = initial;
    This->value = initial;
    This->previous_value = initial;
    This->final_value = initial;
    This->lower_bound = -INFINITY;
    This->upper_bound = INFINITY;
    This->rounding_mode = UI_ANIMATION_ROUNDING_NEAREST;

    *obj = &This->IUIAnimationVariable_iface;

    return S_OK;
}

static HRESULT animation_var_set_value( struct animation_var *var, IUIAnimationStoryboard *storyboard, DOUBLE value )
{
    DOUBLE clamped = value;
    INT32 prev_int, new_int;

    if (isnan( clamped ))
        return S_OK;

    if (clamped < var->lower_bound) clamped = var->lower_bound;
    if (clamped > var->upper_bound) clamped = var->upper_bound;

    prev_int = animation_var_round( var, var->value );
    var->previous_value = var->value;
    var->value = clamped;
    var->final_value = clamped;
    new_int = animation_var_round( var, var->value );

    IUIAnimationVariable_AddRef( &var->IUIAnimationVariable_iface );

    if (var->previous_value != var->value && var->change_handler)
    {
        IUIAnimationVariableChangeHandler *handler = var->change_handler;
        IUIAnimationVariableChangeHandler_AddRef( handler );
        IUIAnimationVariableChangeHandler_OnValueChanged( handler, storyboard, &var->IUIAnimationVariable_iface,
                                                          var->value, var->previous_value );
        IUIAnimationVariableChangeHandler_Release( handler );
    }

    if (prev_int != new_int && var->int_change_handler)
    {
        IUIAnimationVariableIntegerChangeHandler *handler = var->int_change_handler;
        IUIAnimationVariableIntegerChangeHandler_AddRef( handler );
        IUIAnimationVariableIntegerChangeHandler_OnIntegerValueChanged( handler, storyboard,
                                                                        &var->IUIAnimationVariable_iface,
                                                                        new_int, prev_int );
        IUIAnimationVariableIntegerChangeHandler_Release( handler );
    }

    IUIAnimationVariable_Release( &var->IUIAnimationVariable_iface );
    return S_OK;
}

/***********************************************************************
 *          IUIAnimationManager
 */
struct manager
{
    IUIAnimationManager IUIAnimationManager_iface;
    LONG ref;

    UI_ANIMATION_MODE animation_mode;
};

static inline struct manager *impl_from_IUIAnimationManager( IUIAnimationManager *iface )
{
    return CONTAINING_RECORD( iface, struct manager, IUIAnimationManager_iface );
}

static HRESULT WINAPI manager_QueryInterface( IUIAnimationManager *iface, REFIID iid, void **obj )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationManager ))
    {
        IUIAnimationManager_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI manager_AddRef( IUIAnimationManager *iface )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI manager_Release( IUIAnimationManager *iface )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
        free( This );

    return ref;
}

static HRESULT WINAPI manager_CreateAnimationVariable( IUIAnimationManager *iface, DOUBLE initial_value, IUIAnimationVariable **variable )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    TRACE( "(%p)->(%f, %p)\n", This, initial_value, variable );
    if (!variable) return E_POINTER;
    return animation_var_create( initial_value, variable );
}

static HRESULT WINAPI manager_ScheduleTransition( IUIAnimationManager *iface, IUIAnimationVariable *variable,
        IUIAnimationTransition *transition, UI_ANIMATION_SECONDS current_time )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    IUIAnimationStoryboard *storyboard = NULL;
    UI_ANIMATION_SCHEDULING_RESULT result;
    HRESULT hr;

    TRACE( "(%p)->(%p, %p, %f)\n", This, variable, transition, current_time );

    if (!variable || !transition)
        return E_POINTER;

    hr = animation_storyboard_create( &storyboard );
    if (FAILED(hr))
        return hr;

    hr = IUIAnimationStoryboard_AddTransition( storyboard, variable, transition );
    if (SUCCEEDED(hr))
        hr = IUIAnimationStoryboard_Schedule( storyboard, current_time, &result );

    IUIAnimationStoryboard_Release( storyboard );
    return hr;
}

static HRESULT WINAPI manager_CreateStoryboard( IUIAnimationManager *iface, IUIAnimationStoryboard **storyboard )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    TRACE( "(%p)->(%p)\n", This, storyboard );
    if (!storyboard) return E_POINTER;
    return animation_storyboard_create( storyboard );
}

static HRESULT WINAPI manager_FinishAllStoryboards( IUIAnimationManager *iface, UI_ANIMATION_SECONDS max_time )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%f)\n", This, max_time );
    return S_OK;
}

static HRESULT WINAPI manager_AbandonAllStoryboards( IUIAnimationManager *iface )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->()\n", This );
    return S_OK;
}

static HRESULT WINAPI manager_Update( IUIAnimationManager *iface, UI_ANIMATION_SECONDS cur_time, UI_ANIMATION_UPDATE_RESULT *update_result )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "partial stub (%p)->(%f, %p)\n", This, cur_time, update_result );
    if (update_result)
        *update_result = This->animation_mode == UI_ANIMATION_MODE_DISABLED
                ? UI_ANIMATION_UPDATE_NO_CHANGE
                : UI_ANIMATION_UPDATE_VARIABLES_CHANGED;
    return S_OK;
}

static HRESULT WINAPI manager_GetVariableFromTag( IUIAnimationManager *iface, IUnknown *object, UINT32 id, IUIAnimationVariable **variable )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p, %u, %p)\n", This, object, id, variable );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_GetStoryboardFromTag( IUIAnimationManager *iface, IUnknown *object, UINT32 id, IUIAnimationStoryboard **storyboard )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p, %u, %p)\n", This, object, id, storyboard );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_GetStatus( IUIAnimationManager *iface, UI_ANIMATION_MANAGER_STATUS *status )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    TRACE( "(%p)->(%p)\n", This, status );
    if (!status)
        return E_POINTER;
    *status = UI_ANIMATION_MANAGER_IDLE;
    return S_OK;
}

static HRESULT WINAPI manager_SetAnimationMode( IUIAnimationManager *iface, UI_ANIMATION_MODE mode )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );

    TRACE( "(%p)->(%d)\n", This, mode );

    switch (mode)
    {
    case UI_ANIMATION_MODE_DISABLED:
        This->animation_mode = UI_ANIMATION_MODE_DISABLED;
        return S_OK;
    case UI_ANIMATION_MODE_SYSTEM_DEFAULT:
    case UI_ANIMATION_MODE_ENABLED:
        WARN( "Animation mode %d not supported, using disabled mode.\n", mode );
        This->animation_mode = UI_ANIMATION_MODE_DISABLED;
        return S_OK;
    default:
        WARN( "Unexpected animation mode %d.\n", mode );
        return E_INVALIDARG;
    }
}

static HRESULT WINAPI manager_Pause( IUIAnimationManager *iface )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->()\n", This );
    return S_OK;
}

static HRESULT WINAPI manager_Resume( IUIAnimationManager *iface )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->()\n", This );
    return S_OK;
}

static HRESULT WINAPI manager_SetManagerEventHandler( IUIAnimationManager *iface, IUIAnimationManagerEventHandler *handler )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p)\n", This, handler );
    return S_OK;
}

static HRESULT WINAPI manager_SetCancelPriorityComparison( IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p)\n", This, comparison );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetTrimPriorityComparison( IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p)\n", This, comparison );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetCompressPriorityComparison( IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison)
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p)\n", This, comparison );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetConcludePriorityComparison( IUIAnimationManager *iface, IUIAnimationPriorityComparison *comparison )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%p)\n", This, comparison );
    return E_NOTIMPL;
}

static HRESULT WINAPI manager_SetDefaultLongestAcceptableDelay( IUIAnimationManager *iface, UI_ANIMATION_SECONDS delay )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->(%f)\n", This, delay );
    return S_OK;
}

static HRESULT WINAPI manager_Shutdown( IUIAnimationManager *iface )
{
    struct manager *This = impl_from_IUIAnimationManager( iface );
    FIXME( "stub (%p)->()\n", This );
    return S_OK;
}

static const struct IUIAnimationManagerVtbl manager_vtbl =
{
    manager_QueryInterface,
    manager_AddRef,
    manager_Release,
    manager_CreateAnimationVariable,
    manager_ScheduleTransition,
    manager_CreateStoryboard,
    manager_FinishAllStoryboards,
    manager_AbandonAllStoryboards,
    manager_Update,
    manager_GetVariableFromTag,
    manager_GetStoryboardFromTag,
    manager_GetStatus,
    manager_SetAnimationMode,
    manager_Pause,
    manager_Resume,
    manager_SetManagerEventHandler,
    manager_SetCancelPriorityComparison,
    manager_SetTrimPriorityComparison,
    manager_SetCompressPriorityComparison,
    manager_SetConcludePriorityComparison,
    manager_SetDefaultLongestAcceptableDelay,
    manager_Shutdown
};

static HRESULT manager_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct manager *This = calloc( 1, sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->IUIAnimationManager_iface.lpVtbl = &manager_vtbl;
    This->ref = 1;
    This->animation_mode = UI_ANIMATION_MODE_DISABLED;

    hr = IUIAnimationManager_QueryInterface( &This->IUIAnimationManager_iface, iid, obj );

    IUIAnimationManager_Release( &This->IUIAnimationManager_iface );
    return hr;
}

/***********************************************************************
 *          IUIAnimationTimer
 */
struct timer
{
    IUIAnimationTimer IUIAnimationTimer_iface;
    LONG ref;

    BOOL enabled;
    ULONGLONG start_ticks;
};

static inline struct timer *impl_from_IUIAnimationTimer( IUIAnimationTimer *iface )
{
    return CONTAINING_RECORD( iface, struct timer, IUIAnimationTimer_iface );
}

static HRESULT WINAPI timer_QueryInterface( IUIAnimationTimer *iface, REFIID iid, void **obj )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationTimer ))
    {
        IUIAnimationTimer_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI timer_AddRef( IUIAnimationTimer *iface )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI timer_Release( IUIAnimationTimer *iface )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
        free( This );

    return ref;
}

static HRESULT WINAPI timer_SetTimerUpdateHandler( IUIAnimationTimer *iface,
        IUIAnimationTimerUpdateHandler *update_handler,
        UI_ANIMATION_IDLE_BEHAVIOR idle_behaviour )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    FIXME( "stub (%p)->(%p, %d)\n", This, update_handler, idle_behaviour );
    return E_NOTIMPL;
}

static HRESULT WINAPI timer_SetTimerEventHandler( IUIAnimationTimer *iface,
        IUIAnimationTimerEventHandler *handler )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    FIXME( "stub (%p)->(%p)\n", This, handler );
    return S_OK;
}

static HRESULT WINAPI timer_Enable( IUIAnimationTimer *iface )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    FIXME( "partial stub (%p)->()\n", This );
    if (This->enabled)
        return S_FALSE;
    This->enabled = TRUE;
    return S_OK;
}

static HRESULT WINAPI timer_Disable( IUIAnimationTimer *iface )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    FIXME( "partial stub (%p)->()\n", This );
    if (!This->enabled)
        return S_FALSE;
    This->enabled = FALSE;
    return S_OK;
}

static HRESULT WINAPI timer_IsEnabled( IUIAnimationTimer *iface )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    TRACE( "(%p)->()\n", This );
    return This->enabled ? S_OK : S_FALSE;
}

static HRESULT WINAPI timer_GetTime( IUIAnimationTimer *iface, UI_ANIMATION_SECONDS *seconds )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    TRACE( "(%p)->(%p)\n", This, seconds );
    if (!seconds)
        return E_POINTER;
    *seconds = (GetTickCount64() - This->start_ticks) / 1000.0;
    return S_OK;
}

static HRESULT WINAPI timer_SetFrameRateThreshold( IUIAnimationTimer *iface, UINT32 frames_per_sec )
{
    struct timer *This = impl_from_IUIAnimationTimer( iface );
    FIXME( "stub (%p)->(%u)\n", This, frames_per_sec );
    return S_OK;
}

static const struct IUIAnimationTimerVtbl timer_vtbl =
{
    timer_QueryInterface,
    timer_AddRef,
    timer_Release,
    timer_SetTimerUpdateHandler,
    timer_SetTimerEventHandler,
    timer_Enable,
    timer_Disable,
    timer_IsEnabled,
    timer_GetTime,
    timer_SetFrameRateThreshold,
};

static HRESULT timer_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct timer *This = calloc( 1, sizeof(*This) );
    HRESULT hr;

    if (!This)
        return E_OUTOFMEMORY;
    This->IUIAnimationTimer_iface.lpVtbl = &timer_vtbl;
    This->ref = 1;
    This->enabled = FALSE;
    This->start_ticks = GetTickCount64();

    hr = IUIAnimationTimer_QueryInterface( &This->IUIAnimationTimer_iface, iid, obj );

    IUIAnimationTimer_Release( &This->IUIAnimationTimer_iface );
    return hr;
}

/***********************************************************************
 *          IUIAnimationTransition
 */
static inline struct transition *impl_from_IUIAnimationTransition( IUIAnimationTransition *iface )
{
    return CONTAINING_RECORD( iface, struct transition, IUIAnimationTransition_iface );
}

static HRESULT WINAPI transition_QueryInterface( IUIAnimationTransition *iface, REFIID iid, void **obj )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationTransition ))
    {
        IUIAnimationTransition_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI transition_AddRef( IUIAnimationTransition *iface )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI transition_Release( IUIAnimationTransition *iface )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
        free( This );

    return ref;
}

static HRESULT WINAPI transition_SetInitialValue( IUIAnimationTransition *iface, DOUBLE value )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );

    TRACE( "(%p)->(%f)\n", This, value );

    This->initial_value = value;
    return S_OK;
}

static HRESULT WINAPI transition_SetInitialVelocity( IUIAnimationTransition *iface, DOUBLE velocity )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );

    TRACE( "(%p)->(%f)\n", This, velocity );

    This->initial_velocity = velocity;
    return S_OK;
}

static HRESULT WINAPI transition_IsDurationKnown( IUIAnimationTransition *iface )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );

    TRACE( "(%p)\n", This );

    return This->has_duration ? S_OK : S_FALSE;
}

static HRESULT WINAPI transition_GetDuration( IUIAnimationTransition *iface, UI_ANIMATION_SECONDS *duration )
{
    struct transition *This = impl_from_IUIAnimationTransition( iface );

    TRACE( "(%p)->(%p)\n", This, duration );

    if (!duration)
        return E_POINTER;

    if (!This->has_duration)
    {
        *duration = 0.0;
        return UI_E_VALUE_NOT_DETERMINED;
    }

    *duration = This->duration;
    return S_OK;
}

static const struct IUIAnimationTransitionVtbl transition_vtbl =
{
    transition_QueryInterface,
    transition_AddRef,
    transition_Release,
    transition_SetInitialValue,
    transition_SetInitialVelocity,
    transition_IsDurationKnown,
    transition_GetDuration
};

static struct transition *unsafe_impl_from_IUIAnimationTransition( IUIAnimationTransition *iface )
{
    if (!iface || iface->lpVtbl != &transition_vtbl)
        return NULL;
    return CONTAINING_RECORD( iface, struct transition, IUIAnimationTransition_iface );
}

static HRESULT transition_create( BOOL has_final_value, DOUBLE final_value, BOOL has_duration,
        UI_ANIMATION_SECONDS duration, IUIAnimationTransition **obj )
{
    struct transition *This;

    if (!obj)
        return E_POINTER;

    *obj = NULL;
    This = calloc( 1, sizeof(*This) );
    if (!This)
        return E_OUTOFMEMORY;

    This->IUIAnimationTransition_iface.lpVtbl = &transition_vtbl;
    This->ref = 1;
    This->has_final_value = has_final_value;
    This->final_value = final_value;
    This->has_duration = has_duration;
    This->duration = duration;

    *obj = &This->IUIAnimationTransition_iface;
    return S_OK;
}

/***********************************************************************
 *          IUIAnimationTransitionFactory
 */
struct tr_factory
{
    IUIAnimationTransitionFactory IUIAnimationTransitionFactory_iface;
    LONG ref;
};

static inline struct tr_factory *impl_from_IUIAnimationTransitionFactory( IUIAnimationTransitionFactory *iface )
{
    return CONTAINING_RECORD( iface, struct tr_factory, IUIAnimationTransitionFactory_iface );
}

static HRESULT WINAPI tr_factory_QueryInterface( IUIAnimationTransitionFactory *iface, REFIID iid, void **obj )
{
    struct tr_factory *This = impl_from_IUIAnimationTransitionFactory( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationTransitionFactory ))
    {
        IUIAnimationTransitionFactory_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI tr_factory_AddRef( IUIAnimationTransitionFactory *iface )
{
    struct tr_factory *This = impl_from_IUIAnimationTransitionFactory( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI tr_factory_Release( IUIAnimationTransitionFactory *iface )
{
    struct tr_factory *This = impl_from_IUIAnimationTransitionFactory( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
        free( This );

    return ref;
}

static HRESULT WINAPI tr_factory_CreateTransition(IUIAnimationTransitionFactory *iface,
          IUIAnimationInterpolator *interpolator, IUIAnimationTransition **transition)
{
    struct tr_factory *This = impl_from_IUIAnimationTransitionFactory( iface );
    FIXME( "stub (%p)->(%p, %p)\n", This, interpolator, transition );
    return E_NOTIMPL;
}

static const struct IUIAnimationTransitionFactoryVtbl tr_factory_vtbl =
{
    tr_factory_QueryInterface,
    tr_factory_AddRef,
    tr_factory_Release,
    tr_factory_CreateTransition
};

static HRESULT transition_factory_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct tr_factory *This = calloc( 1, sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->IUIAnimationTransitionFactory_iface.lpVtbl = &tr_factory_vtbl;
    This->ref = 1;

    hr = IUIAnimationTransitionFactory_QueryInterface( &This->IUIAnimationTransitionFactory_iface, iid, obj );

    IUIAnimationTransitionFactory_Release( &This->IUIAnimationTransitionFactory_iface );
    return hr;
}

/***********************************************************************
 *          IUITransitionLibrary
 */
struct tr_library
{
    IUIAnimationTransitionLibrary IUIAnimationTransitionLibrary_iface;
    LONG ref;
};

static inline struct tr_library *impl_from_IUIAnimationTransitionLibrary( IUIAnimationTransitionLibrary *iface )
{
    return CONTAINING_RECORD( iface, struct tr_library, IUIAnimationTransitionLibrary_iface );
}

static HRESULT WINAPI tr_library_QueryInterface( IUIAnimationTransitionLibrary *iface,
                                                 REFIID iid, void **obj )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IUIAnimationTransitionLibrary ))
    {
        IUIAnimationTransitionLibrary_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI tr_library_AddRef( IUIAnimationTransitionLibrary *iface )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI tr_library_Release( IUIAnimationTransitionLibrary *iface )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    ULONG ref = InterlockedDecrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
        free( This );

    return ref;
}

static HRESULT WINAPI tr_library_CreateInstantaneousTransition( IUIAnimationTransitionLibrary *iface,
        double final_value, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    TRACE( "(%p)->(%f, %p)\n", This, final_value, transition );
    return transition_create( TRUE, final_value, TRUE, 0.0, transition );
}

static HRESULT WINAPI tr_library_CreateConstantTransition( IUIAnimationTransitionLibrary *iface,
        double duration, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %p)\n", This, duration, transition );
    return transition_create( FALSE, 0.0, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateDiscreteTransition( IUIAnimationTransitionLibrary *iface,
        double delay, double final_value, double hold, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %f, %p)\n", This, delay, final_value, hold, transition );
    return transition_create( TRUE, final_value, TRUE, delay + hold, transition );
}

static HRESULT WINAPI tr_library_CreateLinearTransition( IUIAnimationTransitionLibrary *iface,
        double duration, double final_value, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %p)\n", This, duration, final_value, transition );
    return transition_create( TRUE, final_value, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateLinearTransitionFromSpeed( IUIAnimationTransitionLibrary *iface,
        double speed, double final_value, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %p)\n", This, speed, final_value, transition );
    return transition_create( TRUE, final_value, FALSE, 0.0, transition );
}

static HRESULT WINAPI tr_library_CreateSinusoidalTransitionFromVelocity( IUIAnimationTransitionLibrary *iface,
        double duration, double period, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %p)\n", This, duration, period, transition );
    return transition_create( FALSE, 0.0, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateSinusoidalTransitionFromRange( IUIAnimationTransitionLibrary *iface,
        double duration, double minimum_value, double maximum_value, double period,
        UI_ANIMATION_SLOPE slope, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    DOUBLE final_value = slope == UI_ANIMATION_SLOPE_INCREASING ? maximum_value : minimum_value;

    FIXME( "partial stub (%p)->(%f, %f, %f, %f, %d, %p)\n", This, duration, minimum_value, maximum_value,
           period, slope, transition );
    return transition_create( TRUE, final_value, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateAccelerateDecelerateTransition( IUIAnimationTransitionLibrary *iface,
        double duration, double final_value, double acceleration_ratio, double deceleration_ratio,
        IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %f, %f, %p)\n", This, duration, final_value,
           acceleration_ratio, deceleration_ratio, transition );
    return transition_create( TRUE, final_value, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateReversalTransition( IUIAnimationTransitionLibrary *iface, double duration,
        IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %p)\n", This, duration, transition );
    return transition_create( FALSE, 0.0, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateCubicTransition( IUIAnimationTransitionLibrary *iface, double duration,
        double final_value, double final_velocity, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %f, %p)\n", This, duration, final_value, final_velocity, transition );
    return transition_create( TRUE, final_value, TRUE, duration, transition );
}

static HRESULT WINAPI tr_library_CreateSmoothStopTransition( IUIAnimationTransitionLibrary *iface,
        double maximum_duration, double final_value, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %p)\n", This, maximum_duration, final_value, transition );
    return transition_create( TRUE, final_value, FALSE, 0.0, transition );
}

static HRESULT WINAPI tr_library_CreateParabolicTransitionFromAcceleration( IUIAnimationTransitionLibrary *iface,
        double final_value, double final_velocity, double acceleration, IUIAnimationTransition **transition )
{
    struct tr_library *This = impl_from_IUIAnimationTransitionLibrary( iface );
    FIXME( "partial stub (%p)->(%f, %f, %f, %p)\n", This, final_value, final_velocity, acceleration, transition );
    return transition_create( TRUE, final_value, FALSE, 0.0, transition );
}

static const struct IUIAnimationTransitionLibraryVtbl tr_library_vtbl =
{
    tr_library_QueryInterface,
    tr_library_AddRef,
    tr_library_Release,
    tr_library_CreateInstantaneousTransition,
    tr_library_CreateConstantTransition,
    tr_library_CreateDiscreteTransition,
    tr_library_CreateLinearTransition,
    tr_library_CreateLinearTransitionFromSpeed,
    tr_library_CreateSinusoidalTransitionFromVelocity,
    tr_library_CreateSinusoidalTransitionFromRange,
    tr_library_CreateAccelerateDecelerateTransition,
    tr_library_CreateReversalTransition,
    tr_library_CreateCubicTransition,
    tr_library_CreateSmoothStopTransition,
    tr_library_CreateParabolicTransitionFromAcceleration,
};

static HRESULT library_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct tr_library *This = calloc( 1, sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->IUIAnimationTransitionLibrary_iface.lpVtbl = &tr_library_vtbl;
    This->ref = 1;

    hr = IUIAnimationTransitionLibrary_QueryInterface( &This->IUIAnimationTransitionLibrary_iface, iid, obj );

    IUIAnimationTransitionLibrary_Release( &This->IUIAnimationTransitionLibrary_iface );
    return hr;
}

static struct class_factory manager_cf = { { &class_factory_vtbl }, manager_create };
static struct class_factory timer_cf   = { { &class_factory_vtbl }, timer_create };
static struct class_factory transition_cf = { { &class_factory_vtbl }, transition_factory_create };
static struct class_factory library_cf = { { &class_factory_vtbl }, library_create };

/******************************************************************
 *             DllGetClassObject
 */
HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID iid, void **obj )
{
    IClassFactory *cf = NULL;

    TRACE( "(%s %s %p)\n", debugstr_guid( clsid ), debugstr_guid( iid ), obj );

    if (IsEqualCLSID( clsid, &CLSID_UIAnimationManager ))
        cf = &manager_cf.IClassFactory_iface;
    else if (IsEqualCLSID( clsid, &CLSID_UIAnimationTimer ))
        cf = &timer_cf.IClassFactory_iface;
    else if (IsEqualCLSID( clsid, &CLSID_UIAnimationTransitionFactory ))
        cf = &transition_cf.IClassFactory_iface;
    else if (IsEqualCLSID( clsid, &CLSID_UIAnimationTransitionLibrary ))
        cf = &library_cf.IClassFactory_iface;

    if (!cf)
        return CLASS_E_CLASSNOTAVAILABLE;

    return IClassFactory_QueryInterface( cf, iid, obj );
}
