#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#ifdef _DEBUG

	#include <iostream>
#include <d3d12sdklayers.h>

#endif

void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list va_list;
	va_start(va_list, format);
	vprintf(format, va_list);
	va_end(va_list);
#endif
}

LRESULT WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if(msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

IDXGIFactory6* dxgiFactory = nullptr;
ID3D12Device* device = nullptr;
ID3D12CommandAllocator* cmd_allocator = nullptr;
ID3D12GraphicsCommandList* cmd_list = nullptr;
ID3D12CommandQueue* cmd_queue = nullptr;
IDXGISwapChain4* swap_chain = nullptr;

void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

#ifdef _DEBUG

int main() {
#else
	int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#endif
	DebugOutputFormatString("Show window test.");
	// generating window
	HINSTANCE hInst = GetModuleHandle(nullptr);
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC) WindowProcedure;
	w.lpszClassName = _T("DirectXTest");
	w.hInstance = GetModuleHandle(0);
	RegisterClassEx(&w);

	RECT wrc = {0, 0, window_width, window_height};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	HWND hwnd = CreateWindow(
			w.lpszClassName,
			_T("DX12 テスト"),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			wrc.right - wrc.left,
			wrc.bottom - wrc.top,
			nullptr, // parent window
			nullptr, // menu handle
			w.hInstance,
			nullptr // 追加パラメータ
	);
#ifdef _DEBUG
	EnableDebugLayer();
#endif

	D3D_FEATURE_LEVEL levels[] = {
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
	};
	HRESULT result = S_OK;
	if(FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory)))) {
		if(FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)))) {
			return -1;
		}
	}
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adapter: adapters) {
		DXGI_ADAPTER_DESC a_desc = {};
		adapter->GetDesc(&a_desc);
		std::wstring strDesc = a_desc.Description;
		if(strDesc.find(L"NVIDA") != std::string::npos) {
			tmpAdapter = adapter;
			break;
		}
	}

	// D3Dデバイスの初期化
	D3D_FEATURE_LEVEL feature_level;
	for (auto l: levels) {
		if(D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&device)) == S_OK) {
			feature_level = l;
			break;
		}
	}

	result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_allocator));
	result = device
			->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocator, nullptr, IID_PPV_ARGS(&cmd_list));
	D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
	cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし
	cmd_queue_desc.NodeMask = 0;
	cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // コマンドリストと合わせる
	result = device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue));

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.Width = window_width;
	swap_chain_desc.Height = window_height;
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.Stereo = false;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	result = dxgiFactory->CreateSwapChainForHwnd(
			cmd_queue,
			hwnd,
			&swap_chain_desc,
			nullptr,
			nullptr,
			(IDXGISwapChain1**) &swap_chain
	);

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // Render Target View
	heap_desc.NodeMask = 0;
	heap_desc.NumDescriptors = 2; // バッファリング用で2枚必要
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ID3D12DescriptorHeap* rtv_heaps = nullptr;
	result = device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heaps));
	DXGI_SWAP_CHAIN_DESC swap_chain_desc_rtv = {};
	result = swap_chain->GetDesc(&swap_chain_desc_rtv);

	std::vector<ID3D12Resource*> back_buffers(swap_chain_desc_rtv.BufferCount);
	auto handle = rtv_heaps->GetCPUDescriptorHandleForHeapStart();
	for (auto i = 0; i < swap_chain_desc_rtv.BufferCount; ++i) {
		result = swap_chain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&back_buffers[i]));
		device->CreateRenderTargetView(back_buffers[i], nullptr, handle);
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	ID3D12Fence* fence = nullptr;
	UINT64 fence_val = 0;
	result = device->CreateFence(fence_val, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	ShowWindow(hwnd, SW_SHOW);

	//todo chapter4 追加
	DirectX::XMFLOAT3 vertices[] = {
			{-0.4f, -0.7f, 0.0f}, // 左下
			{-0.4f, 0.7f,  0.0f}, // 左上
			{0.4f,  -0.7f, 0.0f}, // 右下
			{0.4f,  0.7f,  0.0f}, // 右上
	};

	D3D12_HEAP_PROPERTIES heap_prop = {};
	heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC res_desc = {};
	res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	res_desc.Width = sizeof(vertices);
	res_desc.Height = 1;
	res_desc.DepthOrArraySize = 1;
	res_desc.MipLevels = 1;
	res_desc.Format = DXGI_FORMAT_UNKNOWN;
	res_desc.SampleDesc.Count = 1;
	res_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vert_buff = nullptr;
	result = device->CreateCommittedResource(
			&heap_prop,
			D3D12_HEAP_FLAG_NONE,
			&res_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vert_buff)
	);

	DirectX::XMFLOAT3* vert_map = nullptr;
	result = vert_buff->Map(0, nullptr, (void**) &vert_map); // メモリをマップ
	std::copy(std::begin(vertices), std::end(vertices), vert_map); // メモリに頂点生データをコピー
	vert_buff->Unmap(0, nullptr); // メモリのマップを解除

	D3D12_VERTEX_BUFFER_VIEW vb_view = {};
	vb_view.BufferLocation = vert_buff->GetGPUVirtualAddress();
	vb_view.SizeInBytes = sizeof(vertices);
	vb_view.StrideInBytes = sizeof(vertices[0]);
	unsigned short indices[] = {
			0, 1, 2,
			2, 1, 3
	};

	ID3D12Resource* idx_buff = nullptr;
	res_desc.Width = sizeof(indices); // リソースデスクリプタを使いまわしする
	result = device->CreateCommittedResource(
			&heap_prop,
			D3D12_HEAP_FLAG_NONE,
			&res_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&idx_buff)
	);
	// バッファにインデックスデータをコピー
	unsigned short* mapped_idx = nullptr;
	idx_buff->Map(0, nullptr, (void**) &mapped_idx);
	std::copy(std::begin(indices), std::end(indices), mapped_idx);
	idx_buff->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
	index_buffer_view.BufferLocation = idx_buff->GetGPUVirtualAddress();
	index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
	index_buffer_view.SizeInBytes = sizeof(indices);

	//シェーダ読み込み
	ID3DBlob* vs_blob = nullptr;
	ID3DBlob* ps_blob = nullptr;
	ID3DBlob* error_blob = nullptr;

	result = D3DCompileFromFile(
			L"BasicVertexShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"BasicVS",
			"vs_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			&vs_blob, &error_blob
	);
	if(FAILED(result)) {
		if(result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("file not found");
		} else {
			std::string err_str;
			err_str.resize(error_blob->GetBufferSize());
			std::copy_n((char*) error_blob->GetBufferPointer(), error_blob->GetBufferSize(), err_str.begin());
			err_str += "\n";
			OutputDebugStringA(err_str.c_str());
		}
		exit(1);
	}

	result = D3DCompileFromFile(
			L"BasicPixelShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"BasicPS",
			"ps_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			&ps_blob, &error_blob
	);
	if(FAILED(result)) {
		if(result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("file not found");
		} else {
			std::string err_str;
			err_str.resize(error_blob->GetBufferSize());
			std::copy_n((char*) error_blob->GetBufferPointer(), error_blob->GetBufferSize(), err_str.begin());
			err_str += "\n";
			OutputDebugStringA(err_str.c_str());
		}
		exit(1);
	}


	D3D12_INPUT_ELEMENT_DESC input_layout[] = {
			{
					"POSITION",
					0,
					DXGI_FORMAT_R32G32B32_FLOAT,
					0,
					D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
					0
			},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC g_pipeline = {};
	g_pipeline.pRootSignature = nullptr;
	g_pipeline.VS.pShaderBytecode = vs_blob->GetBufferPointer();
	g_pipeline.VS.BytecodeLength = vs_blob->GetBufferSize();
	g_pipeline.PS.pShaderBytecode = ps_blob->GetBufferPointer();
	g_pipeline.PS.BytecodeLength = ps_blob->GetBufferSize();
	g_pipeline.SampleMask = 0xffffffff; // D3D12_DEFAULT_SAMPL_MASKのかわり
	g_pipeline.BlendState.AlphaToCoverageEnable = false;
	g_pipeline.BlendState.IndependentBlendEnable = false;
	D3D12_RENDER_TARGET_BLEND_DESC rt_blend_desc = {};
	rt_blend_desc.BlendEnable = false;
	rt_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	rt_blend_desc.LogicOpEnable = false;
	g_pipeline.BlendState.RenderTarget[0] = rt_blend_desc;
	g_pipeline.RasterizerState.MultisampleEnable = false;
	g_pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	g_pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	g_pipeline.RasterizerState.DepthClipEnable = true;

	g_pipeline.RasterizerState.FrontCounterClockwise = false;
	g_pipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	g_pipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	g_pipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	g_pipeline.RasterizerState.AntialiasedLineEnable = false;
	g_pipeline.RasterizerState.ForcedSampleCount = 0;
	g_pipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	g_pipeline.DepthStencilState.DepthEnable = false;
	g_pipeline.DepthStencilState.StencilEnable = false;

	g_pipeline.InputLayout.pInputElementDescs = input_layout;
	g_pipeline.InputLayout.NumElements = _countof(input_layout);

	g_pipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	g_pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	g_pipeline.NumRenderTargets = 1;
	g_pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	g_pipeline.SampleDesc.Count = 1;
	g_pipeline.SampleDesc.Quality = 0;


	ID3D12RootSignature* root_signature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* root_sig_blob = nullptr;
	result = D3D12SerializeRootSignature(&root_signature_desc,
	                                     D3D_ROOT_SIGNATURE_VERSION_1_0,
	                                     &root_sig_blob,
	                                     &error_blob);
	result = device->CreateRootSignature(0,
	                                     root_sig_blob->GetBufferPointer(),
	                                     root_sig_blob->GetBufferSize(),
	                                     IID_PPV_ARGS(&root_signature));
	root_sig_blob->Release();

	g_pipeline.pRootSignature = root_signature;
	ID3D12PipelineState* pipeline_state = nullptr;
	result = device->CreateGraphicsPipelineState(&g_pipeline, IID_PPV_ARGS(&pipeline_state));

	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;
	viewport.Height = window_height;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissor_rect = {};
	scissor_rect.top = 0;
	scissor_rect.left = 0;
	scissor_rect.right = scissor_rect.left + window_width;
	scissor_rect.bottom = scissor_rect.top + window_height;


	unsigned int frame = 0;
	//todo chapter4 ここまで
	MSG msg = {};
	while (true) {
		if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if(msg.message == WM_QUIT) {
			break;
		}

		auto bb_index = swap_chain->GetCurrentBackBufferIndex();
		D3D12_RESOURCE_BARRIER barrier_desc = {};
		barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier_desc.Transition.pResource = back_buffers[bb_index];
		barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		cmd_list->ResourceBarrier(1, &barrier_desc);

		cmd_list->SetPipelineState(pipeline_state); // todo chapter4

		auto rtv_heap = rtv_heaps->GetCPUDescriptorHandleForHeapStart();
		rtv_heap.ptr += static_cast<ULONG_PTR>(bb_index
		                                       * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		cmd_list->OMSetRenderTargets(1, &rtv_heap, false, nullptr);

		// todo chapter4
		float r, g, b;
		r = (float) (0xff & frame >> 16) / 255.0f;
		r = (float) (0xff & frame >> 8) / 255.0f;
		r = (float) (0xff & frame >> 0) / 255.0f;
		float clear_color[] = {r, g, b, 1.0f};
		cmd_list->ClearRenderTargetView(rtv_heap, clear_color, 0, nullptr);
		frame++;
		cmd_list->RSSetViewports(1, &viewport);
		cmd_list->RSSetScissorRects(1, &scissor_rect);
		cmd_list->SetGraphicsRootSignature(root_signature);

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd_list->IASetVertexBuffers(0, 1, &vb_view);
		cmd_list->IASetIndexBuffer(&index_buffer_view);

		cmd_list->DrawIndexedInstanced(6, 1, 0, 0, 0);
		// todo chapter4 ここまで

		barrier_desc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier_desc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		cmd_list->ResourceBarrier(1, &barrier_desc);

		cmd_list->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmd_lists[] = {cmd_list};
		cmd_queue->ExecuteCommandLists(1, cmd_lists);
		// 完了まで待つ
		cmd_queue->Signal(fence, ++fence_val);
		if(fence->GetCompletedValue() != fence_val) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fence_val, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		cmd_allocator->Reset(); //キューをクリア
		cmd_list->Reset(cmd_allocator, nullptr); //コマンドリストを空に

		// flip
		swap_chain->Present(1, 0);
	}

	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}