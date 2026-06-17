#pragma once
#include <fstream>
#include <SDL.h>

void scc(auto* ptr, std::string msg){
    if(!ptr){
        fprintf(stderr, "%s: %s", msg, SDL_GetError());
        exit(1);
    }
}

void scc(bool t, std::string msg){
    if(!t) {
        fprintf(stderr,"%s: %s", msg, SDL_GetError());
        exit(1);
    }
}