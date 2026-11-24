/*
 * Copyright 2020 Nikolay Sivov for CodeWeavers
 * Copyright 2025 Paint.NET Wine compatibility contributors
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

#include "initguid.h"
#include "private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dcomp);

#define ASYNC_STATUS_CLOSED 4

HRESULT WINAPI DCompositionCreateDevice( IDXGIDevice *dxgi_device, REFIID iid, void **device )
{
    FIXME( "%p, %s, %p.\n", dxgi_device, debugstr_guid( iid ), device );

    return E_NOTIMPL;
}

HRESULT WINAPI DCompositionCreateDevice2( IUnknown *rendering_device, REFIID iid, void **device )
{
    FIXME( "%p, %s, %p.\n", rendering_device, debugstr_guid( iid ), device );

    return E_NOTIMPL;
}

HRESULT WINAPI DCompositionCreateDevice3( IUnknown *rendering_device, REFIID iid, void **device )
{
    FIXME( "%p, %s, %p.\n", rendering_device, debugstr_guid( iid ), device );

    return E_NOTIMPL;
}

HRESULT WINAPI DCompositionBoostCompositorClock( BOOL enable )
{
    FIXME( "%d: stub!\n", enable );

    return E_NOTIMPL;
}

HRESULT WINAPI CreatePresentationFactory( IUnknown *device, REFIID iid, void **factory )
{
    FIXME( "%p, %s, %p: stub!\n", device, debugstr_guid( iid ), factory );

    if (factory) *factory = NULL;
    return E_NOTIMPL;
}

/***********************************************************************
 *          Completed IAsyncAction for CompositorController
 */
struct completed_async_action
{
    IAsyncAction IAsyncAction_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;

    AsyncStatus status;
    IAsyncActionCompletedHandler *handler;
};

static inline struct completed_async_action *impl_from_IAsyncAction( IAsyncAction *iface )
{
    return CONTAINING_RECORD( iface, struct completed_async_action, IAsyncAction_iface );
}

static HRESULT WINAPI completed_async_action_QueryInterface( IAsyncAction *iface, REFIID iid, void **out )
{
    struct completed_async_action *impl = impl_from_IAsyncAction( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IAsyncAction ))
    {
        *out = &impl->IAsyncAction_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IAsyncInfo ))
    {
        *out = &impl->IAsyncInfo_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI completed_async_action_AddRef( IAsyncAction *iface )
{
    struct completed_async_action *impl = impl_from_IAsyncAction( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI completed_async_action_Release( IAsyncAction *iface )
{
    struct completed_async_action *impl = impl_from_IAsyncAction( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );

    if (!ref)
    {
        if (impl->handler) IAsyncActionCompletedHandler_Release( impl->handler );
        free( impl );
    }

    return ref;
}

static HRESULT WINAPI completed_async_action_GetIids( IAsyncAction *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI completed_async_action_GetRuntimeClassName( IAsyncAction *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    if (!class_name) return E_POINTER;
    return WindowsCreateString( L"Windows.Foundation.IAsyncAction",
                                wcslen( L"Windows.Foundation.IAsyncAction" ), class_name );
}

static HRESULT WINAPI completed_async_action_GetTrustLevel( IAsyncAction *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI completed_async_action_put_Completed( IAsyncAction *iface, IAsyncActionCompletedHandler *handler )
{
    struct completed_async_action *impl = impl_from_IAsyncAction( iface );

    TRACE( "iface %p, handler %p.\n", iface, handler );

    if (impl->status == ASYNC_STATUS_CLOSED) return E_ILLEGAL_METHOD_CALL;
    if (impl->handler) return E_ILLEGAL_DELEGATE_ASSIGNMENT;

    if ((impl->handler = handler))
    {
        IAsyncAction_AddRef( iface );
        IAsyncActionCompletedHandler_AddRef( handler );
        IAsyncActionCompletedHandler_Invoke( handler, iface, impl->status );
        IAsyncAction_Release( iface );
    }

    return S_OK;
}

static HRESULT WINAPI completed_async_action_get_Completed( IAsyncAction *iface, IAsyncActionCompletedHandler **handler )
{
    struct completed_async_action *impl = impl_from_IAsyncAction( iface );

    TRACE( "iface %p, handler %p.\n", iface, handler );

    if (!handler) return E_POINTER;
    if (impl->status == ASYNC_STATUS_CLOSED) return E_ILLEGAL_METHOD_CALL;

    *handler = impl->handler;
    if (*handler) IAsyncActionCompletedHandler_AddRef( *handler );
    return S_OK;
}

static HRESULT WINAPI completed_async_action_GetResults( IAsyncAction *iface )
{
    struct completed_async_action *impl = impl_from_IAsyncAction( iface );

    TRACE( "iface %p.\n", iface );

    if (impl->status == ASYNC_STATUS_CLOSED) return E_ILLEGAL_METHOD_CALL;
    return S_OK;
}

static const struct IAsyncActionVtbl completed_async_action_vtbl =
{
    /* IUnknown methods */
    completed_async_action_QueryInterface,
    completed_async_action_AddRef,
    completed_async_action_Release,
    /* IInspectable methods */
    completed_async_action_GetIids,
    completed_async_action_GetRuntimeClassName,
    completed_async_action_GetTrustLevel,
    /* IAsyncAction methods */
    completed_async_action_put_Completed,
    completed_async_action_get_Completed,
    completed_async_action_GetResults,
};

DEFINE_IINSPECTABLE( completed_async_info, IAsyncInfo, struct completed_async_action, IAsyncAction_iface )

static HRESULT WINAPI completed_async_info_get_Id( IAsyncInfo *iface, unsigned __int32 *id )
{
    struct completed_async_action *impl = impl_from_IAsyncInfo( iface );

    TRACE( "iface %p, id %p.\n", iface, id );

    if (!id) return E_POINTER;
    if (impl->status == ASYNC_STATUS_CLOSED) return E_ILLEGAL_METHOD_CALL;
    *id = 1;
    return S_OK;
}

static HRESULT WINAPI completed_async_info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct completed_async_action *impl = impl_from_IAsyncInfo( iface );

    TRACE( "iface %p, status %p.\n", iface, status );

    if (!status) return E_POINTER;
    if (impl->status == ASYNC_STATUS_CLOSED) return E_ILLEGAL_METHOD_CALL;
    *status = impl->status;
    return S_OK;
}

static HRESULT WINAPI completed_async_info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error_code )
{
    struct completed_async_action *impl = impl_from_IAsyncInfo( iface );

    TRACE( "iface %p, error_code %p.\n", iface, error_code );

    if (!error_code) return E_POINTER;
    if (impl->status == ASYNC_STATUS_CLOSED) return E_ILLEGAL_METHOD_CALL;
    *error_code = S_OK;
    return S_OK;
}

static HRESULT WINAPI completed_async_info_Cancel( IAsyncInfo *iface )
{
    TRACE( "iface %p.\n", iface );
    return S_OK;
}

static HRESULT WINAPI completed_async_info_Close( IAsyncInfo *iface )
{
    struct completed_async_action *impl = impl_from_IAsyncInfo( iface );

    TRACE( "iface %p.\n", iface );

    impl->status = ASYNC_STATUS_CLOSED;
    if (impl->handler)
    {
        IAsyncActionCompletedHandler_Release( impl->handler );
        impl->handler = NULL;
    }
    return S_OK;
}

static const struct IAsyncInfoVtbl completed_async_info_vtbl =
{
    /* IUnknown methods */
    completed_async_info_QueryInterface,
    completed_async_info_AddRef,
    completed_async_info_Release,
    /* IInspectable methods */
    completed_async_info_GetIids,
    completed_async_info_GetRuntimeClassName,
    completed_async_info_GetTrustLevel,
    /* IAsyncInfo methods */
    completed_async_info_get_Id,
    completed_async_info_get_Status,
    completed_async_info_get_ErrorCode,
    completed_async_info_Cancel,
    completed_async_info_Close,
};

static HRESULT completed_async_action_create( IAsyncAction **out )
{
    struct completed_async_action *impl;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->IAsyncAction_iface.lpVtbl = &completed_async_action_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &completed_async_info_vtbl;
    impl->ref = 1;
    impl->status = Completed;

    *out = &impl->IAsyncAction_iface;
    return S_OK;
}

/***********************************************************************
 *          Windows.UI.Composition.Compositor
 */
struct compositor
{
    ICompositor ICompositor_iface;
    ICompositorInterop ICompositorInterop_iface;
    IClosable IClosable_iface;
    LONG ref;
};

static inline struct compositor *impl_from_ICompositor( ICompositor *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor_iface );
}

static HRESULT WINAPI compositor_QueryInterface( ICompositor *iface, REFIID iid, void **out )
{
    struct compositor *impl = impl_from_ICompositor( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_ICompositor ))
    {
        *out = &impl->ICompositor_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_ICompositorInterop ))
    {
        *out = &impl->ICompositorInterop_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IClosable ))
    {
        *out = &impl->IClosable_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI compositor_AddRef( ICompositor *iface )
{
    struct compositor *impl = impl_from_ICompositor( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI compositor_Release( ICompositor *iface )
{
    struct compositor *impl = impl_from_ICompositor( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI compositor_GetIids( ICompositor *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_GetRuntimeClassName( ICompositor *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    if (!class_name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Compositor,
                                wcslen( RuntimeClass_Windows_UI_Composition_Compositor ), class_name );
}

static HRESULT WINAPI compositor_GetTrustLevel( ICompositor *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI compositor_CreateColorKeyFrameAnimation( ICompositor *iface, IColorKeyFrameAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateColorBrush( ICompositor *iface, ICompositionColorBrush **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateColorBrushWithColor( ICompositor *iface, Color color, ICompositionColorBrush **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateContainerVisual( ICompositor *iface, IContainerVisual **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateCubicBezierEasingFunction( ICompositor *iface, Vector2 point1, Vector2 point2,
        ICubicBezierEasingFunction **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateEffectFactory( ICompositor *iface, IGraphicsEffect *effect,
        ICompositionEffectFactory **result )
{
    FIXME( "iface %p, effect %p, result %p stub!\n", iface, effect, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateEffectFactoryWithProperties( ICompositor *iface, IGraphicsEffect *effect,
        IIterable_HSTRING *animatable, ICompositionEffectFactory **result )
{
    FIXME( "iface %p, effect %p, animatable %p, result %p stub!\n", iface, effect, animatable, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateExpressionAnimation( ICompositor *iface, IExpressionAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateExpressionAnimationWithExpression( ICompositor *iface, HSTRING expression,
        IExpressionAnimation **result )
{
    FIXME( "iface %p, expression %s, result %p stub!\n", iface, debugstr_hstring( expression ), result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateInsetClip( ICompositor *iface, IInsetClip **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateInsetClipWithInsets( ICompositor *iface, FLOAT left, FLOAT top, FLOAT right,
        FLOAT bottom, IInsetClip **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateLinearEasingFunction( ICompositor *iface, ILinearEasingFunction **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreatePropertySet( ICompositor *iface, ICompositionPropertySet **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateQuaternionKeyFrameAnimation( ICompositor *iface, IQuaternionKeyFrameAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateScalarKeyFrameAnimation( ICompositor *iface, IScalarKeyFrameAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateScopedBatch( ICompositor *iface, CompositionBatchTypes type,
        ICompositionScopedBatch **result )
{
    FIXME( "iface %p, type %d, result %p stub!\n", iface, type, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateSpriteVisual( ICompositor *iface, ISpriteVisual **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateSurfaceBrush( ICompositor *iface, ICompositionSurfaceBrush **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateSurfaceBrushWithSurface( ICompositor *iface, ICompositionSurface *surface,
        ICompositionSurfaceBrush **result )
{
    FIXME( "iface %p, surface %p, result %p stub!\n", iface, surface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateTargetForCurrentView( ICompositor *iface, ICompositionTarget **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateVector2KeyFrameAnimation( ICompositor *iface, IVector2KeyFrameAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateVector3KeyFrameAnimation( ICompositor *iface, IVector3KeyFrameAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_CreateVector4KeyFrameAnimation( ICompositor *iface, IVector4KeyFrameAnimation **result )
{
    FIXME( "iface %p, result %p stub!\n", iface, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_GetCommitBatch( ICompositor *iface, CompositionBatchTypes type,
        ICompositionCommitBatch **result )
{
    FIXME( "iface %p, type %d, result %p stub!\n", iface, type, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static const struct ICompositorVtbl compositor_vtbl =
{
    /* IUnknown methods */
    compositor_QueryInterface,
    compositor_AddRef,
    compositor_Release,
    /* IInspectable methods */
    compositor_GetIids,
    compositor_GetRuntimeClassName,
    compositor_GetTrustLevel,
    /* ICompositor methods */
    compositor_CreateColorKeyFrameAnimation,
    compositor_CreateColorBrush,
    compositor_CreateColorBrushWithColor,
    compositor_CreateContainerVisual,
    compositor_CreateCubicBezierEasingFunction,
    compositor_CreateEffectFactory,
    compositor_CreateEffectFactoryWithProperties,
    compositor_CreateExpressionAnimation,
    compositor_CreateExpressionAnimationWithExpression,
    compositor_CreateInsetClip,
    compositor_CreateInsetClipWithInsets,
    compositor_CreateLinearEasingFunction,
    compositor_CreatePropertySet,
    compositor_CreateQuaternionKeyFrameAnimation,
    compositor_CreateScalarKeyFrameAnimation,
    compositor_CreateScopedBatch,
    compositor_CreateSpriteVisual,
    compositor_CreateSurfaceBrush,
    compositor_CreateSurfaceBrushWithSurface,
    compositor_CreateTargetForCurrentView,
    compositor_CreateVector2KeyFrameAnimation,
    compositor_CreateVector3KeyFrameAnimation,
    compositor_CreateVector4KeyFrameAnimation,
    compositor_GetCommitBatch,
};

static inline struct compositor *impl_from_ICompositorInterop( ICompositorInterop *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositorInterop_iface );
}

static HRESULT WINAPI compositor_interop_QueryInterface( ICompositorInterop *iface, REFIID iid, void **out )
{
    struct compositor *impl = impl_from_ICompositorInterop( iface );
    return ICompositor_QueryInterface( &impl->ICompositor_iface, iid, out );
}

static ULONG WINAPI compositor_interop_AddRef( ICompositorInterop *iface )
{
    struct compositor *impl = impl_from_ICompositorInterop( iface );
    return ICompositor_AddRef( &impl->ICompositor_iface );
}

static ULONG WINAPI compositor_interop_Release( ICompositorInterop *iface )
{
    struct compositor *impl = impl_from_ICompositorInterop( iface );
    return ICompositor_Release( &impl->ICompositor_iface );
}

static HRESULT WINAPI compositor_interop_CreateCompositionSurfaceForHandle( ICompositorInterop *iface, HANDLE swapchain,
        ICompositionSurface **result )
{
    FIXME( "iface %p, swapchain %p, result %p stub!\n", iface, swapchain, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_interop_CreateCompositionSurfaceForSwapChain( ICompositorInterop *iface, IUnknown *swapchain,
        ICompositionSurface **result )
{
    FIXME( "iface %p, swapchain %p, result %p stub!\n", iface, swapchain, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_interop_CreateGraphicsDevice( ICompositorInterop *iface, IUnknown *device,
        ICompositionGraphicsDevice **result )
{
    FIXME( "iface %p, device %p, result %p stub!\n", iface, device, result );
    if (result) *result = NULL;
    return E_NOTIMPL;
}

static const struct ICompositorInteropVtbl compositor_interop_vtbl =
{
    /* IUnknown methods */
    compositor_interop_QueryInterface,
    compositor_interop_AddRef,
    compositor_interop_Release,
    /* ICompositorInterop methods */
    compositor_interop_CreateCompositionSurfaceForHandle,
    compositor_interop_CreateCompositionSurfaceForSwapChain,
    compositor_interop_CreateGraphicsDevice,
};

DEFINE_IINSPECTABLE_( compositor_closable, IClosable, struct compositor, compositor_impl_from_IClosable, IClosable_iface, &impl->ICompositor_iface )

static HRESULT WINAPI compositor_closable_Close( IClosable *iface )
{
    FIXME( "iface %p stub!\n", iface );
    return S_OK;
}

static const struct IClosableVtbl compositor_closable_vtbl =
{
    /* IUnknown methods */
    compositor_closable_QueryInterface,
    compositor_closable_AddRef,
    compositor_closable_Release,
    /* IInspectable methods */
    compositor_closable_GetIids,
    compositor_closable_GetRuntimeClassName,
    compositor_closable_GetTrustLevel,
    /* IClosable methods */
    compositor_closable_Close,
};

static HRESULT compositor_create( ICompositor **out )
{
    struct compositor *impl;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->ICompositor_iface.lpVtbl = &compositor_vtbl;
    impl->ICompositorInterop_iface.lpVtbl = &compositor_interop_vtbl;
    impl->IClosable_iface.lpVtbl = &compositor_closable_vtbl;
    impl->ref = 1;

    *out = &impl->ICompositor_iface;
    return S_OK;
}

/***********************************************************************
 *          ICompositorController
 */
struct compositor_controller
{
    ICompositorController ICompositorController_iface;
    IClosable IClosable_iface;
    LONG ref;

    ICompositor *compositor;
    INT64 next_token;
};

static inline struct compositor_controller *impl_from_ICompositorController( ICompositorController *iface )
{
    return CONTAINING_RECORD( iface, struct compositor_controller, ICompositorController_iface );
}

static HRESULT WINAPI compositor_controller_QueryInterface( ICompositorController *iface, REFIID iid, void **out )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_ICompositorController ))
    {
        *out = &impl->ICompositorController_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IClosable ))
    {
        *out = &impl->IClosable_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI compositor_controller_AddRef( ICompositorController *iface )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI compositor_controller_Release( ICompositorController *iface )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p, ref %lu.\n", iface, ref );

    if (!ref)
    {
        if (impl->compositor) ICompositor_Release( impl->compositor );
        free( impl );
    }

    return ref;
}

static HRESULT WINAPI compositor_controller_GetIids( ICompositorController *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI compositor_controller_GetRuntimeClassName( ICompositorController *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p.\n", iface, class_name );
    if (!class_name) return E_POINTER;
    return WindowsCreateString( RuntimeClass_Windows_UI_Composition_Core_CompositorController,
                                wcslen( RuntimeClass_Windows_UI_Composition_Core_CompositorController ), class_name );
}

static HRESULT WINAPI compositor_controller_GetTrustLevel( ICompositorController *iface, TrustLevel *trust_level )
{
    TRACE( "iface %p, trust_level %p.\n", iface, trust_level );
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_get_Compositor( ICompositorController *iface, ICompositor **value )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    HRESULT hr;

    FIXME( "iface %p, value %p semi-stub.\n", iface, value );

    if (!value) return E_POINTER;
    *value = NULL;

    if (!impl->compositor && FAILED(hr = compositor_create( &impl->compositor )))
        return hr;

    *value = impl->compositor;
    ICompositor_AddRef( *value );
    return S_OK;
}

static HRESULT WINAPI compositor_controller_Commit( ICompositorController *iface )
{
    FIXME( "iface %p stub!\n", iface );
    return S_OK;
}

static HRESULT WINAPI compositor_controller_EnsurePreviousCommitCompletedAsync( ICompositorController *iface,
        IAsyncAction **operation )
{
    FIXME( "iface %p, operation %p semi-stub.\n", iface, operation );
    if (!operation) return E_POINTER;
    return completed_async_action_create( operation );
}

static HRESULT WINAPI compositor_controller_add_CommitNeeded( ICompositorController *iface,
        ITypedEventHandler_CompositorController_IInspectable *handler, EventRegistrationToken *token )
{
    struct compositor_controller *impl = impl_from_ICompositorController( iface );
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    if (!handler || !token) return E_POINTER;
    token->value = ++impl->next_token;
    return S_OK;
}

static HRESULT WINAPI compositor_controller_remove_CommitNeeded( ICompositorController *iface,
        EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64d stub!\n", iface, token.value );
    return S_OK;
}

static const struct ICompositorControllerVtbl compositor_controller_vtbl =
{
    /* IUnknown methods */
    compositor_controller_QueryInterface,
    compositor_controller_AddRef,
    compositor_controller_Release,
    /* IInspectable methods */
    compositor_controller_GetIids,
    compositor_controller_GetRuntimeClassName,
    compositor_controller_GetTrustLevel,
    /* ICompositorController methods */
    compositor_controller_get_Compositor,
    compositor_controller_Commit,
    compositor_controller_EnsurePreviousCommitCompletedAsync,
    compositor_controller_add_CommitNeeded,
    compositor_controller_remove_CommitNeeded,
};

DEFINE_IINSPECTABLE_( compositor_controller_closable, IClosable, struct compositor_controller, controller_impl_from_IClosable, IClosable_iface, &impl->ICompositorController_iface )

static HRESULT WINAPI compositor_controller_closable_Close( IClosable *iface )
{
    struct compositor_controller *impl = controller_impl_from_IClosable( iface );

    FIXME( "iface %p stub!\n", iface );

    if (impl->compositor)
    {
        ICompositor_Release( impl->compositor );
        impl->compositor = NULL;
    }
    return S_OK;
}

static const struct IClosableVtbl compositor_controller_closable_vtbl =
{
    /* IUnknown methods */
    compositor_controller_closable_QueryInterface,
    compositor_controller_closable_AddRef,
    compositor_controller_closable_Release,
    /* IInspectable methods */
    compositor_controller_closable_GetIids,
    compositor_controller_closable_GetRuntimeClassName,
    compositor_controller_closable_GetTrustLevel,
    /* IClosable methods */
    compositor_controller_closable_Close,
};

static HRESULT compositor_controller_create( IInspectable **out )
{
    struct compositor_controller *impl;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->ICompositorController_iface.lpVtbl = &compositor_controller_vtbl;
    impl->IClosable_iface.lpVtbl = &compositor_controller_closable_vtbl;
    impl->ref = 1;

    *out = (IInspectable *)&impl->ICompositorController_iface;
    return S_OK;
}

/***********************************************************************
 *          CompositorController Activation Factory
 */
struct compositor_controller_factory
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
};

static inline struct compositor_controller_factory *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct compositor_controller_factory, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct compositor_controller_factory *impl = impl_from_IActivationFactory( iface );

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

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct compositor_controller_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct compositor_controller_factory *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    TRACE( "iface %p, instance %p.\n", iface, instance );
    if (!instance) return E_POINTER;
    return compositor_controller_create( instance );
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    /* IUnknown methods */
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

static struct compositor_controller_factory compositor_controller_factory =
{
    {&factory_vtbl},
    1,
};

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *buffer = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "class %s, factory %p.\n", debugstr_hstring( classid ), factory );

    if (!factory) return E_INVALIDARG;
    *factory = NULL;

    if (buffer && !wcscmp( buffer, RuntimeClass_Windows_UI_Composition_Core_CompositorController ))
        return IActivationFactory_QueryInterface( &compositor_controller_factory.IActivationFactory_iface,
                                                  &IID_IActivationFactory, (void **)factory );

    return CLASS_E_CLASSNOTAVAILABLE;
}
