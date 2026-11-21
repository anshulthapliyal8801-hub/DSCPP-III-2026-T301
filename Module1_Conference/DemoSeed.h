// MUNSphere — Module1_Conference/DemoSeed.h
#pragma once

#include "API.h"
#include "Auth.h"

namespace mun {

// Fills an EMPTY data folder with a realistic demo conference through the
// real API. Returns 0 on success; prints the logins to use.
int seedDemo(Router& router, Auth& auth);

}  // namespace mun
