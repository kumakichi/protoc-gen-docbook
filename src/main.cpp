#include <iostream>
#include "docbook_generator.h"
#include <google/protobuf/compiler/plugin.h>

using namespace google::protobuf::compiler::docbook;

int main(int argc, char* argv[]) {
	//MyCodeGenerator generator;
	DocbookGenerator dbg;
	return google::protobuf::compiler::PluginMain(argc, argv, &dbg);
}