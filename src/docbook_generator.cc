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

// Author: askldjd@gmail.com
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include "docbook_generator.h"
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>

//#include <Windows.h>

namespace google { namespace protobuf { namespace compiler { namespace docbook {

	namespace utils {

		std::string Trim(const std::string& str)
		{
			std::string s = str;
			unsigned int p;
			while ((p = s.length()) > 0 && (unsigned char)s[p-1] <= ' ')
				s.resize(p-1);
			while ((p = s.length()) > 0 && (unsigned char)s[0] <= ' ')
				s.erase(0, 1);
			return s;
		}

		//! @details
		//! This method parses a Java like .properties file that is k=v pairs.
		//!
		//! @param[in] filePath
		//! File path to the property file.
		//!
		//! @return
		//! k/v pairs that represents the property file.
		//!
		//! @remark
		//! Hate to reinvent the wheel here since there are about a million 
		//! parser implementations out there. However, I hate to bring in Boost 
		//! library just for something this trivial.
		std::map <std::string, std::string> ParseProperty(std::string const &filePath)
		{
			std::map <std::string, std::string> options;
			std::ifstream ifs(filePath.c_str(), std::ifstream::in);
			if(ifs.is_open())
			{
				std::string line;
				while(std::getline(ifs, line))
				{
					line = Trim(line);

					// First non-space char that is '#' is a comment line.
					if(line.empty() == false && line[0]=='#') 
						continue;

					int p = 0;
					if ((p = (int)line.find('=')) > 0)
					{
						std::string k = Trim(line.substr(0, p));
						std::string v = Trim(line.substr(p+1));
						if (!k.empty() && !v.empty())
							options[k] = v;
					}
				}
			}

			ifs.close();
			return options;
		}
	}

	namespace {

		const char *OPTION_NAME_FIELD_NAME_COLUMN_WIDTH = "field_name_column_width";
		const char *OPTION_NAME_FIELD_TYPE_COLUMN_WIDTH = "field_type_column_width";
		const char *OPTION_NAME_FIELD_RULE_COLUMN_WIDTH = "field_rules_column_width";
		const char *OPTION_NAME_FIELD_DESC_COLUMN_WIDTH = "field_desc_column_width";
		const char *OPTION_NAME_COLUMN_HEADER_COLOR = "column_header_color";


		const char *DEFAULT_OUTPUT_NAME = "docbook_out.xml";

		const char *DEFAULT_INSERTION_POINT= "insertion_point";

		const char *DEFAULT_FIELD_NAME_COLUMN_WIDTH = "4";
		const char *DEFAULT_FIELD_TYPE_COLUMN_WIDTH = "3";
		const char *DEFAULT_FIELD_RULES_COLUMN_WIDTH = "3";
		const char *DEFAULT_FIELD_DESC_COLUMN_WIDTH = "8";
		const char *DEFAULT_COLUMN_HEADER_COLOR = "8eb4e3";

		//! @details
		//! Turn comments into docbook paragraph form by replacing 
		//! every 2 newlines into a paragraph.
		//! Experimental, doesn't seem to work too well in all scenarios.
		std::string ParagraphFormatComment(std::string const &comment)
		{
			size_t index = 0;
			std::string paragraph  = "<para>";
			paragraph += comment;
			while (true) {
				index = paragraph.find("\n\n", index);
				if (index == string::npos)
					break;

				paragraph.replace(index, 2, "</para>\n<para>");

				index += 3;
			}
			paragraph += "</para>";
			return paragraph;
		}

		//! Docbook header that wraps the document under the Article tag.
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

		//! Docbook footer that closes the article tag
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
			std::string const &description,
			std::map<std::string,std::string> const &options)
		{
			std::map<std::string,std::string>::const_iterator itr;

			os 
				<< "<sect1>"
				<< "<title> Message: " << title << "</title>" << std::endl
				<< "<para>" << description << "</para>" << std::endl
				<< "<informaltable frame=\"all\"" << std::endl
				<< " xml:id=\"" << xmlID << "\">" << std::endl
				<< "<tgroup cols=\"4\">" << std::endl
				<< " <colspec colname=\"c1\" colnum=\"1\" colwidth=\"";

			itr = options.find(OPTION_NAME_FIELD_NAME_COLUMN_WIDTH);
			if(itr != options.end())
				os << itr->second;
			else
				os << DEFAULT_FIELD_NAME_COLUMN_WIDTH;

			os
				<< "*\" />"<< std::endl
				<< "<colspec colname=\"c2\" colnum=\"2\""
				<< " colwidth=\"";

			itr = options.find(OPTION_NAME_FIELD_TYPE_COLUMN_WIDTH);
			if(itr != options.end())
				os << itr->second << std::endl;
			else
				os << DEFAULT_FIELD_TYPE_COLUMN_WIDTH;

			os
				<< "*\" />" << std::endl
				<< "<colspec colname=\"c3\" colnum=\"3\""
				<< " colwidth=\"";

			itr = options.find(OPTION_NAME_FIELD_RULE_COLUMN_WIDTH);
			if(itr != options.end())
				os << itr->second << std::endl;
			else
				os << DEFAULT_FIELD_RULES_COLUMN_WIDTH;

			os
				<< "*\" />" << std::endl
				<< "<colspec colname=\"c4\" colnum=\"4\""
				<< " colwidth=\"";

			itr = options.find(OPTION_NAME_FIELD_DESC_COLUMN_WIDTH);
			if(itr != options.end())
				os << itr->second << std::endl;
			else
				os << DEFAULT_FIELD_DESC_COLUMN_WIDTH;

			os
				<<"*\" />" << std::endl
				<< "<thead>" << std::endl
				<< "<row>" << std::endl
				<< "<?dbhtml bgcolor=\"#";

			itr = options.find(OPTION_NAME_COLUMN_HEADER_COLOR);
			if(itr != options.end())
				os << itr->second << std::endl;
			else
				os << DEFAULT_COLUMN_HEADER_COLOR;

			os
				<<"\" ?>"<< std::endl
				<< "<?dbfo bgcolor=\"#";
			itr = options.find(OPTION_NAME_COLUMN_HEADER_COLOR);
			if(itr != options.end())
				os << itr->second << std::endl;
			else
				os << DEFAULT_COLUMN_HEADER_COLOR;
			os
				<<"\" ?>"<< std::endl
				<< "\t<entry>Element</entry>" << std::endl
				<< "\t<entry>Type</entry>"<< std::endl
				<< "\t<entry>Occurs</entry>"<< std::endl
				<< "\t<entry>Description</entry>"<< std::endl
				<< "</row>"<< std::endl
				<< "</thead>"<< std::endl
				<< "<tbody>"<< std::endl;
		}
		void WriteInformalTableFooter(std::ostringstream &os)
		{
			os 
				<< "</tbody>"<< std::endl
				<< "</tgroup>"<< std::endl
				<< "</informaltable>"<< std::endl
				<< "</sect1>"<< std::endl;
		}

		void WriteInformalTableEntry(
			std::ostringstream &os, 
			string const &fieldname,
			string const &type,
			string const &occurrence,
			string const &comment)
		{
			std::string paragraphComment = ParagraphFormatComment(comment);
			os 
				<< "<row>"<<std::endl
				<< "\t<entry>" << fieldname << "</entry>" << std::endl
				<< "\t<entry>" << type << "</entry>" << std::endl
				<< "\t<entry>" << occurrence << "</entry>" << std::endl
				<< "\t<entry>" << paragraphComment << "</entry>" << std::endl
				<< "</row>"<<std::endl
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
			templateOs 
				<< "<!-- @@protoc_insertion_point("
				<<DEFAULT_INSERTION_POINT
				<<") -->" 
				<< std::endl;
			WriteDocbookFooter(templateOs);

			scoped_ptr<io::ZeroCopyOutputStream> output(context.Open(DEFAULT_OUTPUT_NAME));
			io::Printer printer(output.get(), '$');
			printer.PrintRaw(templateOs.str().c_str());
		}
	}	
	static bool templateFileMade = false;

	DocbookGenerator::DocbookGenerator() 
	{
		_options = utils::ParseProperty("docbook.properties");
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
		//Sleep(5000);

		std::ostringstream _docbookStream;
		std::ostringstream os;

		for (int i = 0; i < file->message_type_count(); i++) 
		{
			Descriptor const *messageDescriptor = file->message_type(i);
			string cleanName = messageDescriptor->full_name();

			std::replace(
				cleanName.begin(),
				cleanName.end(),
				'.', 
				'_');

			WriteInformalTableHeader(
				_docbookStream,
				cleanName, 
				messageDescriptor->name(),
				GetDescriptorComment(messageDescriptor),
				_options);

			for (int i = 0; i < messageDescriptor->field_count(); i++) {
				std::string labelStr;

				switch(messageDescriptor->field(i)->label())
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

				if(messageDescriptor->field(i)->type() == FieldDescriptor::TYPE_MESSAGE)
				{
					typeName = MakeXLink(
						messageDescriptor->field(i)->message_type()->full_name(),
						messageDescriptor->field(i)->message_type()->name());
				}
				else
				{
					typeName = messageDescriptor->field(i)->type_name();
				}

				WriteInformalTableEntry(
					_docbookStream, 
					messageDescriptor->field(i)->name(),
					typeName,
					labelStr,
					GetDescriptorComment(messageDescriptor->field(i)));
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
			context->OpenForInsert(DEFAULT_OUTPUT_NAME, DEFAULT_INSERTION_POINT));
		io::Printer printer(output.get(), '$');
		printer.PrintRaw(_docbookStream.str().c_str());

		if (printer.failed()) {
			*error = "CodeGenerator detected write error.";
			return false;
		}

		return true;
	}

}}}}  // end namespace
