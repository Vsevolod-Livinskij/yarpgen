/*
Copyright (c) 2015-2016, Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "namegen.h"

using namespace rl;

std::vector<char> ItNameGen::pieces = {'i', 'j', 'k', 'l', 'm', 'n'};
std::vector<uint64_t> ItNameGen::cnt = {0, 0, 0, 0, 0, 0};

std::vector<char> SclNameGen::pieces = {'s'};
std::vector<uint64_t> SclNameGen::cnt = {0};

std::vector<char> VecNameGen::pieces = {'v'};
std::vector<uint64_t> VecNameGen::cnt = {0};

uint64_t ScopeIdGen::cnt = 0;
