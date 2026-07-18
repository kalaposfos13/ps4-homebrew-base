#include "app.h"

#include <filesystem>
#include <fstream>

const char* dump_path[2] = {"/app0/assets/images/frame_dump1.bin", "/app0/assets/images/frame_dump2.bin"};

enum MemoryProt : u32 {
    NoAccess = 0,
    CpuRead = 1,
    CpuWrite = 2,
    CpuReadWrite = 3,
    CpuExec = 4,
    GpuRead = 16,
    GpuWrite = 32,
    GpuReadWrite = 48,
};
template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}
static VAddr alloc_memory(size_t size, size_t alignment, u32 memType, u32 prot) {
    size = AlignUp(size, 16_KB);
    ASSERT(alignment == AlignUp(alignment, 16_KB));

    off_t phys = 0;
    ASSERT_OK(sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), size, alignment,
                                            memType, &phys));

    void* virt = nullptr;
    ASSERT_OK(sceKernelMapDirectMemory(&virt, size, prot, 0, phys, alignment));

    return VAddr(virt);
}

App::App() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_PAD_TRACKER);

    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("userid: {}, pad handle: {}", user_id, pad_handle);

    int frameID = 0;
    renderer.Init();

    std::string font_path =
        fmt::format("/{}/common/font/DFHEI5-SONY.ttf", sceKernelGetFsSandboxRandomWord());
    ASSERT_MSG(renderer.scene->InitFont(&font, font_path.c_str(), 40) && font != nullptr,
               "Failed to init font");
}

App::~App() {
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

void App::InitCamera() {
    if (use_dumped_frame) {
        dumped_frame_buf[0] = (void*)alloc_memory(1280 * 800 * 2, 0, ORBIS_KERNEL_WB_ONION,
                                                  MemoryProt::CpuReadWrite | MemoryProt::GpuRead);
        std::ifstream is1(dump_path[0], std::ios::binary);
        is1.read(reinterpret_cast<char*>(dumped_frame_buf[0]), 1280 * 800 * 2);

        dumped_frame_buf[1] = (void*)alloc_memory(1280 * 800 * 2, 0, ORBIS_KERNEL_WB_ONION,
                                                  MemoryProt::CpuReadWrite | MemoryProt::GpuRead);
        std::ifstream is2(dump_path[1], std::ios::binary);
        is2.read(reinterpret_cast<char*>(dumped_frame_buf[1]), 1280 * 800 * 2);
    }
}

void App::InitPadTracker() {
    LOG_INFO("Setting up pad tracker");
    LOG_INFO("eg: {} {} {} {}", exposuregain.exposureControl, exposuregain.exposure,
             exposuregain.gain, exposuregain.mode);

    pt_input.handles[0] = pad_handle;
    pt_input.handles[1] = -1;
    pt_input.handles[2] = -1;
    pt_input.handles[3] = -1;

    pt_input.images[0].exposure = exposuregain.exposure;
    pt_input.images[0].gain = exposuregain.gain;
    pt_input.images[0].width = 1280;
    pt_input.images[0].height = 800;
    pt_input.images[1].exposure = exposuregain.exposure;
    pt_input.images[1].gain = exposuregain.gain;
    pt_input.images[1].width = 1280;
    pt_input.images[1].height = 800;

    pt_input.images[0].data = nullptr;
    pt_input.images[1].data = nullptr;

    s32 pt_o_s, pt_g_s;
    ASSERT_OK(scePadTrackerGetWorkingMemorySize(&pt_o_s, &pt_g_s));
    ASSERT(pt_o_s > 0 && pt_g_s > 0);
    LOG_INFO("needed onion size: {:#x}, needed garlic size: {:#x}", pt_o_s, pt_g_s);
    VAddr pt_onion = alloc_memory(pt_o_s, 0, ORBIS_KERNEL_WB_ONION,
                                  MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    VAddr pt_garlic = alloc_memory(pt_g_s, 0, ORBIS_KERNEL_WC_GARLIC,
                                   MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    ASSERT_OK(scePadTrackerInit(pt_onion, pt_garlic, 0, 0));
}

static int dump_next_camera_frame_id = -1;

bool App::UpdateCamera() {
    return true;
}

void App::UpdatePadTracker() {
    if (use_dumped_frame) {
        void* fb =
            state.pt_status[0] == Status::Calibrating ? dumped_frame_buf[0] : dumped_frame_buf[1];
        pt_input.images[0].data = fb;
        pt_input.images[1].data = fb;
    }

    ASSERT_OK(scePadTrackerUpdate(pt_input));
    scePadTrackerReadState(pad_handle, &pt_output);
    for (int i = 0; i < ORBIS_CAMERA_MAX_DEVICE_NUM; i++) {
        switch (pt_output.imageCoordinates[i].status) {
        case ORBIS_PAD_TRACKER_TRACKING:
            state.pt_status[i] = Status::Tracking;
            break;
        case ORBIS_PAD_TRACKER_NOT_TRACKING:
            state.pt_status[i] = Status::NotTracking;
            break;
        case ORBIS_PAD_TRACKER_CALIBRATING:
            state.pt_status[i] = Status::Calibrating;
            break;
        default:
            UNREACHABLE_MSG("pt_status: {}", (u32)pt_output.imageCoordinates[0].status);
            state.pt_status[i] = Status::Error;
        }
    }
}

bool App::HandleInput() {
    scePadReadState(pad_handle, &pdata);
    if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE) != 0) {
        return false;
    }
    static bool sq_pressed = false;
    if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_SQUARE) != 0) {
        if (!sq_pressed) {
            state.eye = 1 - state.eye;
            sq_pressed = true;
        }
    } else {
        sq_pressed = false;
    }
    static bool left_pressed = false;
    if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_LEFT) != 0) {
        if (!left_pressed) {
            dump_next_camera_frame_id = 0;
            left_pressed = true;
        }
    } else {
        left_pressed = false;
    }
    static bool right_pressed = false;
    if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_RIGHT) != 0) {
        if (!right_pressed) {
            dump_next_camera_frame_id = 1;
            right_pressed = true;
        }
    } else {
        right_pressed = false;
    }
    return true;
}

void App::FrameStart() {
    renderer.scene->FrameBufferClear();
}

void App::FrameEnd() {
    renderer.scene->SubmitFlip();
    renderer.scene->FrameWait();
    renderer.scene->FrameBufferSwap();
}

void App::DrawCameraImage() {
    if (use_dumped_frame) {
        static Image output{};
        if (!output.pixels) {
            ASSERT(output.Allocate(1280, 800));
        }
        Camera::ConvertRAW16(dumped_frame_buf[state.pt_status[0] == Status::Calibrating ? 0 : 1],
                            1280, 800, output);
        renderer.DrawImage(output, 0, 0);
        return;
    }
}

void App::DrawPadTrackerResult() {

    if (state.pt_status[1 - state.eye] == Status::Tracking) {
        renderer.scene->DrawRectangle((pt_output.imageCoordinates[1 - state.eye].x * 1280) - 10,
                                      (pt_output.imageCoordinates[1 - state.eye].y * 800) - 10, 20,
                                      20, {0, 255, 0});
    }
    if (state.pt_status[state.eye] == Status::Tracking) {
        renderer.scene->DrawRectangle((pt_output.imageCoordinates[state.eye].x * 1280) - 10,
                                      (pt_output.imageCoordinates[state.eye].y * 800) - 10, 20, 20,
                                      {255, 0, 0});
    }
    if (state.pt_status[state.eye] == Status::Calibrating) {
        DrawLoadingFrame();
    }
}

void App::DrawMoveTrackerResult() {
    if (state.mt_status == Status::Tracking) {
        LOG_INFO("x: {}, y: {}, z: {}", mt_state.position.x, mt_state.position.y,
                 mt_state.position.z);
    }
    std::string pos_text =
        fmt::format("x: {:+07.3f}\ny: {:+07.3f}\nz: {:+07.3f}", mt_state.position.x,
                    mt_state.position.y, mt_state.position.z);
    renderer.scene->DrawText(pos_text.c_str(), font, 50, 850, {0, 0, 0}, {255, 255, 255});

    std::string cam_stats_text = fmt::format("cam pitch: {:+07.3f}, roll: {:+07.3f}",
                                             mt_state.cameraPitchAngle, mt_state.cameraRollAngle);
    renderer.scene->DrawText(cam_stats_text.c_str(), font, 50, 1000, {0, 0, 0}, {255, 255, 255});

    auto const& acc = mt_state.acceleration;
    std::string accel_text =
        fmt::format("dx: {:+07.3f}\ndy: {:+07.3f}\ndz: {:+07.3f}", acc.x, acc.y, acc.z);
    renderer.scene->DrawText(accel_text.c_str(), font, 400, 850, {0, 0, 0}, {255, 255, 255});
}

void App::DrawDebugStuff() {
    std::string debug_text = fmt::format("tracker status: {}", u32(pt_output.imageCoordinates[0].status));
    renderer.scene->DrawText(debug_text.c_str(), font, 1500, 850, {0, 0, 0}, {255, 0, 255});
}

void App::DrawLoadingFrame() {
    renderer.scene->DrawRectangle(400 + 000, 375, 50, 50, {255, 255, 255});
    renderer.scene->DrawRectangle(400 + 200, 375, 50, 50, {255, 255, 255});
    renderer.scene->DrawRectangle(400 + 400, 375, 50, 50, {255, 255, 255});
}
