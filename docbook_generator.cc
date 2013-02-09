// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: askldjd@gmail.com (Alan Ning)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include "docbook_generator.h"
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <iostream>

namespace google { namespace protobuf { namespace compiler { namespace docbook {

	namespace {
		void WriteDocbookHeader(std::ostringstream &os)
		{
			os 
				<< "<?xml version=\"1.0\""
				<< " encoding=\"utf-8\""
				<< " standalone=\"no\"?>"
				<< "<article xmlns=\"http://docbook.org/ns/docbook\""
				<< " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
				<< " version=\"5.0\">" 
				<< std::endl;		
		}

		void WriteDocbookFooter(std::ostringstream &os)
		{
			os 
				<< "</article>"
				<< std::endl;		
		}

		void WriteInformalTableHeader(
			std::ostringstream &os, 
			std::string const &xmlID, 
			std::string const &title,
			std::string const &description)
		{
			os 
				<< "<sect1>"
				<< "<title>" << title << "</title>"
				<< "<para>" << description << "</para>"
				<< "<informaltable frame=\"all\""
				<< " xml:id=\"" << xmlID << "\">"
				<< "<tgroup cols=\"4\">"
				<< " <colspec colname=\"c1\" colnum=\"1\""
				<< " colwidth=\"4*\" />"
				<< "<colspec colname=\"c2\" colnum=\"2\""
				<< " colwidth=\"3*\" />"
				<< "<colspec colname=\"c3\" colnum=\"3\""
				<< " colwidth=\"3*\" />"
				<< "<colspec colname=\"c4\" colnum=\"4\""
				<< " colwidth=\"8*\" />"
				<< "<thead>"
				<< "<row>"
				<< "<?dbhtml bgcolor=\"#8eb4e3\" ?>"
				<< "<?dbfo bgcolor=\"#8eb4e3\" ?>"
				<< "<entry>Element</entry>"
				<< "<entry>Type</entry>"
				<< "<entry>Occurs</entry>"
				<< "<entry>Description</entry>"
				<< "</row>"
				<< "</thead>"
				<< "<tbody>"
				<< std::endl;
		}

		void WriteInformalTableFooter(std::ostringstream &os)
		{
			os 
				<< "</tbody>"
				<< "</tgroup>"
				<< "</informaltable>"
				<< "</sect1>"
				<< std::endl;
		}

		void WriteInformalTableEntry(
			std::ostringstream &os, 
			string const &fieldname,
			string const &type,
			string const &occurrence,
			string const &comment)
		{
			os 
				<< "<row>"
				<< "<entry>" << fieldname << "</entry>"
				<< "<entry>" << type << "</entry>"
				<< "<entry>" << occurrence << "</entry>"
				<< "<entry>" << comment << "</entry>"
				<< "</row>"
				<< std::endl;
		}

		std::string MakeXLink(std::string const &messageName, std::string const &displayName)
		{
			std::ostringstream os;

			std::string refName = messageName;
			std::replace(
				refName.begin(),
				refName.end(),
				'.', 
				'_');


			os 
				<< "<emphasis role=\"underline\""
				<< " xlink:href=\"" << "#" << refName << "\">"
				<< displayName<< "</emphasis>";

			return os.str();
		}

		void MakeTemplateFile(GeneratorContext &context)
		{
			std::ostringstream templateOs;
			WriteDocbookHeader(templateOs);
			templateOs << "<!-- @@protoc_insertion_point(point) -->" << std::endl;
			WriteDocbookFooter(templateOs);

			scoped_ptr<io::ZeroCopyOutputStream> output(context.Open("template2.xml"));
			io::Printer printer(output.get(), '$');
			printer.PrintRaw(templateOs.str().c_str());
		}
	}	
	static bool templateFileMade = false;

	DocbookGenerator::DocbookGenerator() 
	{
	}
	DocbookGenerator::~DocbookGenerator() 
	{
	}


	template <typename DescriptorType>
	static std::string GetDescriptorComment(const DescriptorType* descriptor) {
		SourceLocation location;
		string comments;
		if (descriptor->GetSourceLocation(&location)) {
			comments = location.leading_comments;
			comments += " ";
			comments += location.trailing_comments;
		}

		return comments;
	}

	bool DocbookGenerator::Generate(const FileDescriptor* file,
		const string& parameter,
		GeneratorContext* context,
		string* error) const 
	{
		std::ostringstream _docbookStream;
		std::ostringstream os;

		for (int i = 0; i < file->message_type_count(); i++) 
		{
			Descriptor const *descriptor = file->message_type(i);
			string cleanName = descriptor->full_name();

			std::replace(
				cleanName.begin(),
				cleanName.end(),
				'.', 
				'_');

			WriteInformalTableHeader(
				_docbookStream,
				cleanName, 
				descriptor->name(),
				GetDescriptorComment(descriptor));

			for (int i = 0; i < descriptor->field_count(); i++) {
				std::string labelStr;

				switch(descriptor->field(i)->label())
				{
				case FieldDescriptor::LABEL_OPTIONAL:
					labelStr = "optional";
					break;
				case FieldDescriptor::LABEL_REPEATED:
					labelStr = "repeated";
					break;
				case FieldDescriptor::LABEL_REQUIRED:
					labelStr = "required";
					break;
				}

				std::string typeName;

				if(descriptor->field(i)->type() == FieldDescriptor::TYPE_MESSAGE)
				{
					typeName = MakeXLink(
						descriptor->field(i)->message_type()->full_name(),
						descriptor->field(i)->message_type()->name());
				}
				else
				{
					typeName = descriptor->field(i)->type_name();
				}

				WriteInformalTableEntry(
					_docbookStream, 
					descriptor->field(i)->name(),
					typeName,
					labelStr,
					GetDescriptorComment(descriptor->field(i)));
			}
			WriteInformalTableFooter(_docbookStream);
		}

		//for (int i = 0; i < file->enum_type_count(); i++) {

		//	EnumDescriptor const *descriptor = file->enum_type(i);
		//	std::cout << "message type = " << descriptor->full_name() << std::endl;
		//	WriteDocCommentBody(NULL, descriptor);

		//	for (int i = 0; i < descriptor->value_count(); i++) {
		//		std::cout << "field: " 
		//			<< std::endl
		//			<< "\t"
		//			<<"type = "
		//			<< descriptor->value(i)->number()
		//			<< std::endl
		//			<< "\t"
		//			<< "name = " 
		//			<< descriptor->value(i)->full_name() 
		//			<< std::endl
		//			<< "\t";
		//		WriteDocCommentBody(NULL, descriptor->value(i));
		//	}
		//}

		if(templateFileMade == false) 
		{
			templateFileMade = true;
			MakeTemplateFile(*context);
		}

		scoped_ptr<io::ZeroCopyOutputStream> output(
			context->OpenForInsert("template2.xml","point"));
		io::Printer printer(output.get(), '$');
		printer.PrintRaw(_docbookStream.str().c_str());

		if (printer.failed()) {
			*error = "CodeGenerator detected write error.";
			return false;
		}

		return true;
	}



}  // namespace docbook
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
