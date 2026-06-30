#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>

extern "C" {
    bool ImGui_ImplSDLRenderer3_Init_C(SDL_Renderer* renderer) {
        return ImGui_ImplSDLRenderer3_Init(renderer);
    }
    void ImGui_ImplSDLRenderer3_Shutdown_C() {
        ImGui_ImplSDLRenderer3_Shutdown();
    }
    void ImGui_ImplSDLRenderer3_NewFrame_C() {
        ImGui_ImplSDLRenderer3_NewFrame();
    }
    void ImGui_ImplSDLRenderer3_RenderDrawData_C(ImDrawData* draw_data, SDL_Renderer* renderer) {
        ImGui_ImplSDLRenderer3_RenderDrawData(draw_data, renderer);
    }
}
