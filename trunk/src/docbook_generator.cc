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

// For debugging only
//#include <Windows.h>

namespace google { namespace protobuf { namespace compiler { namespace docbook {

namespace utils {

	//! @details
	//! Helper method to trim the beginning and ending white spaces within 
	//! a string.
	string Trim(const string& str)
	{
		string s = str;
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
	std::map <string, string> ParseProperty(string const &filePath)
	{
		std::map <string, string> options;
		std::ifstream ifs(filePath.c_str(), std::ifstream::in);
		if(ifs.is_open())
		{
			string line;
			while(std::getline(ifs, line))
			{
				line = Trim(line);

				// First non-space char that is '#' is a comment line.
				if(line.empty() == false && line[0]=='#') 
					continue;

				int p = 0;
				if ((p = (int)line.find('=')) > 0)
				{
					string k = Trim(line.substr(0, p));
					string v = Trim(line.substr(p+1));
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

	char const* OPTION_NAME_FIELD_NAME_COLUMN_WIDTH = "field_name_column_width";
	char const *OPTION_NAME_FIELD_TYPE_COLUMN_WIDTH = "field_type_column_width";
	char const *OPTION_NAME_FIELD_RULE_COLUMN_WIDTH = "field_rules_column_width";
	char const *OPTION_NAME_FIELD_DESC_COLUMN_WIDTH = "field_desc_column_width";
	char const *OPTION_NAME_COLUMN_HEADER_COLOR = "column_header_color";

	char const *DEFAULT_OUTPUT_NAME = "docbook_out.xml";

	char const *DEFAULT_INSERTION_POINT= "insertion_point";

	char const *DEFAULT_FIELD_NAME_COLUMN_WIDTH = "4";
	char const *DEFAULT_FIELD_TYPE_COLUMN_WIDTH = "3";
	char const *DEFAULT_FIELD_RULES_COLUMN_WIDTH = "3";
	char const *DEFAULT_FIELD_DESC_COLUMN_WIDTH = "8";
	char const *DEFAULT_COLUMN_HEADER_COLOR = "8eb4e3";
	
	std::map<string, string> s_docbookOptions;

	//! @details
	//! This field marks the first time DocBookGenerator::Generate method is
	//! called. If it is the first time, we need to generate the template 
	//! DocBook file. 
	//!
	//! @remark
	//! In case you are wondering, yes this is a hack. The problem is that
	//! the DocBookGenerator::Generate by const by contract, and therefore
	//! can only be stateless. This is likely because in normal language, 
	//! every .proto file tends to generate its own source file, but 
	//! DocbookGenerator needs to merge them all into the same xml file. The 
	//! nature of this incompatibility results in this hack.
	bool s_templateFileMade = false;

	//! @details
	//! Turn comments into docbook paragraph form by replacing 
	//! every 2 newlines into a paragraph.
	//! Experimental, doesn't seem to work too well in all scenarios.
	string ParagraphFormatComment(string const &comment)
	{
		// Occasionally there are some comments that has nothing but white
		// spaces. This will eliminate those useless fillers and sanitize the
		// comments.
		if(utils::Trim(comment).empty())
		{
			return "";
		}

		size_t index = 0;
		string paragraph  = "<para>";
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

	//! @details
	//! Clean up the comment string for any special characters
	//! to ensure it is acceptable in XML format.
	string SanitizeCommentForXML(string const &comment)
	{
		string cleanedComment;
		cleanedComment.reserve(comment.size());
		for(size_t pos = 0; pos != comment.size(); ++pos) 
		{
			char c = comment[pos];
			switch(c) 
			{
			case '&':  
				cleanedComment.append("&amp;");       
				break;
			case '\"': 
				cleanedComment.append("&quot;");      
				break;
			case '\'': 
				cleanedComment.append("&apos;");      
				break;
			case '<':  
				cleanedComment.append("&lt;");
				break;
			case '>':  
				cleanedComment.append("&gt;");
				break;
			default:
				if(c < 0 || c > 0x7F)
					c = ' ';
				cleanedComment.append(1, c); 
				break;
			}
		}
		return cleanedComment;
	}

	string MakeXLink(string const &messageName, string const &displayName)
	{
		std::ostringstream os;

		string refName = messageName;
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

	string MakeDefaultValueString(FieldDescriptor const *fd)
	{
		std::ostringstream defaultStringOs;
		if(fd->has_default_value())
		{
			defaultStringOs << "\n[default = ";
			switch(fd->type())
			{
			case FieldDescriptor::TYPE_BOOL:
				{
					if(fd->default_value_bool())
						defaultStringOs << "true";
					else 
						defaultStringOs << "false";
				}
				break;
			case FieldDescriptor::TYPE_BYTES:
				break;
			case FieldDescriptor::TYPE_DOUBLE:
				defaultStringOs << fd->default_value_double();
				break;
			case FieldDescriptor::TYPE_ENUM:
				defaultStringOs << fd->default_value_enum()->name();
				break;
			case FieldDescriptor::TYPE_FIXED32:
				defaultStringOs << fd->default_value_uint32();
				break;
			case FieldDescriptor::TYPE_FIXED64:
				defaultStringOs << fd->default_value_uint64();
				break;
			case FieldDescriptor::TYPE_FLOAT:
				defaultStringOs << fd->default_value_float();
				break;
			case FieldDescriptor::TYPE_GROUP:
				break;
			case FieldDescriptor::TYPE_INT32:
				defaultStringOs << fd->default_value_int32();
				break;
			case FieldDescriptor::TYPE_INT64:
				defaultStringOs << fd->default_value_int64();
				break;
			default:
				return "";
			}

			defaultStringOs << " ]";
		}
		return defaultStringOs.str();
	}

	template <typename DescriptorType>
	static string GetDescriptorComment(const DescriptorType* descriptor) {
		SourceLocation location;
		string comments;
		if (descriptor->GetSourceLocation(&location)) {
			comments = location.leading_comments;
			comments += " ";
			comments += location.trailing_comments;
		}

		return comments;
	}

	//! @details
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

	//! @details
	//! Docbook footer that closes the article tag
	void WriteDocbookFooter(std::ostringstream &os)
	{
		os << "</article>" << std::endl;		
	}

	void WriteProtoFileHeader(std::ostringstream &os, FileDescriptor const *fileDescriptor)
	{			
		os << "<sect1>"
			<< "<title> File: " << fileDescriptor->name() << "</title>" << std::endl;
	}

	void WriteProtoFileFooter(std::ostringstream &os)
	{
		os << "</sect1>" << std::endl;
	}

	//! @details
	//! Write the Informal Table Header for a Message type.
	//! This will define the column header, width and style of the
	//! field table.
	void WriteMessageInformalTableHeader(
		std::ostringstream &os, 
		string const &xmlID, 
		string const &title,
		string const &description,
		int sectionLevel)
	{
		std::map<string,string>::const_iterator itr;

		os 
			<< "<sect" << sectionLevel << ">"
			<< "<title> Message: " << title << "</title>" << std::endl
			<< "<para>" << description << "</para>" << std::endl
			<< "<informaltable frame=\"all\""
			<< " xml:id=\"" << xmlID << "\">" << std::endl
			<< "<tgroup cols=\"4\">" << std::endl
			<< " <colspec colname=\"c1\" colnum=\"1\" colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_NAME_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_NAME_COLUMN_WIDTH;

		os
			<< "*\" />"<< std::endl
			<< "<colspec colname=\"c2\" colnum=\"2\""
			<< " colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_TYPE_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_TYPE_COLUMN_WIDTH;

		os
			<< "*\" />" << std::endl
			<< "<colspec colname=\"c3\" colnum=\"3\""
			<< " colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_RULE_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_RULES_COLUMN_WIDTH;

		os
			<< "*\" />" << std::endl
			<< "<colspec colname=\"c4\" colnum=\"4\""
			<< " colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_DESC_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_DESC_COLUMN_WIDTH;

		os
			<<"*\" />" << std::endl
			<< "<thead>" << std::endl
			<< "<row>" << std::endl
			<< "<?dbhtml bgcolor=\"#";

		itr = s_docbookOptions.find(OPTION_NAME_COLUMN_HEADER_COLOR);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_COLUMN_HEADER_COLOR;

		os
			<<"\" ?>"<< std::endl
			<< "<?dbfo bgcolor=\"#";
		itr = s_docbookOptions.find(OPTION_NAME_COLUMN_HEADER_COLOR);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_COLUMN_HEADER_COLOR;
		os
			<<"\" ?>"<< std::endl
			<< "\t<entry>Element</entry>" << std::endl
			<< "\t<entry>Type</entry>"<< std::endl
			<< "\t<entry>Rule</entry>"<< std::endl
			<< "\t<entry>Description</entry>"<< std::endl
			<< "</row>"<< std::endl
			<< "</thead>"<< std::endl
			<< "<tbody>"<< std::endl;
	}

	void WriteEnumInformalTableHeader(
		std::ostringstream &os, 
		string const &xmlID, 
		string const &title,
		string const &description,
		int sectionLevel)
	{
		std::map<string,string>::const_iterator itr;

		os 
			<< "<sect" << sectionLevel << ">"
			<< "<title> Enum: " << title << "</title>" << std::endl
			<< "<para>" << description << "</para>" << std::endl
			<< "<informaltable frame=\"all\""
			<< " xml:id=\"" << xmlID << "\">" << std::endl
			<< "<tgroup cols=\"3\">" << std::endl
			<< " <colspec colname=\"c1\" colnum=\"1\" colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_NAME_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_NAME_COLUMN_WIDTH;

		os
			<< "*\" />"<< std::endl
			<< "<colspec colname=\"c2\" colnum=\"2\""
			<< " colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_TYPE_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_TYPE_COLUMN_WIDTH;

		os
			<< "*\" />" << std::endl
			<< "<colspec colname=\"c3\" colnum=\"3\""
			<< " colwidth=\"";

		itr = s_docbookOptions.find(OPTION_NAME_FIELD_RULE_COLUMN_WIDTH);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_FIELD_RULES_COLUMN_WIDTH;

		os
			<< "*\" />" << std::endl
			<< "<thead>" << std::endl
			<< "<row>" << std::endl
			<< "<?dbhtml bgcolor=\"#";

		itr = s_docbookOptions.find(OPTION_NAME_COLUMN_HEADER_COLOR);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_COLUMN_HEADER_COLOR;

		os
			<<"\" ?>"<< std::endl
			<< "<?dbfo bgcolor=\"#";
		itr = s_docbookOptions.find(OPTION_NAME_COLUMN_HEADER_COLOR);
		if(itr != s_docbookOptions.end())
			os << itr->second;
		else
			os << DEFAULT_COLUMN_HEADER_COLOR;
		os
			<<"\" ?>"<< std::endl
			<< "\t<entry>Element</entry>" << std::endl
			<< "\t<entry>Value</entry>"<< std::endl
			<< "\t<entry>Description</entry>"<< std::endl
			<< "</row>"<< std::endl
			<< "</thead>"<< std::endl
			<< "<tbody>"<< std::endl;
	}

	void WriteInformalTableFooter(std::ostringstream &os, int sectionLevel)
	{
		os 
			<< "</tbody>"<< std::endl
			<< "</tgroup>"<< std::endl
			<< "</informaltable>"<< std::endl
			<< "</sect"<< sectionLevel << ">" <<std::endl;
	}

	void WriteMessageInformalTableEntry(
		std::ostringstream &os, 
		string const &fieldname,
		string const &type,
		string const &occurrence,
		string const &defaultString,
		string const &comment)
	{
		string cleanComment = SanitizeCommentForXML(comment);
		string paragraphComment = ParagraphFormatComment(cleanComment);
		os 
			<< "<row>"<<std::endl
			<< "\t<entry>" << fieldname << "</entry>" << std::endl
			<< "\t<entry>" << type << "</entry>" << std::endl
			<< "\t<entry>" << occurrence << "</entry>" << std::endl
			<< "\t<entry>" << paragraphComment;

		if(defaultString.empty() == false)
		{
			if(paragraphComment.empty())
			{
				os << defaultString << std::endl;
			}
			else
			{
				os << "<para>" << defaultString << "</para>" << std::endl;
			}
		}

		os << "</entry>" << std::endl;
		
		os
			<< "</row>"<<std::endl
			<< std::endl;
	}

	void WriteEnumInformalTableEntry(
		std::ostringstream &os, 
		string const &fieldname,
		int enumValue,
		string const &comment)
	{
		string cleanComment = SanitizeCommentForXML(comment);
		string paragraphComment = ParagraphFormatComment(cleanComment);
		os 
			<< "<row>"<<std::endl
			<< "\t<entry>" << fieldname << "</entry>" << std::endl
			<< "\t<entry>" << enumValue << "</entry>" << std::endl
			<< "\t<entry>" << paragraphComment << "</entry>" << std::endl
			<< "</row>"<<std::endl
			<< std::endl;
	}

	void WriteMessageFieldEntries( 
		std::ostringstream &os, 
		Descriptor const *messageDescriptor)
	{
		for (int i = 0; i < messageDescriptor->field_count(); i++) {
			string labelStr;

			FieldDescriptor const *fd = messageDescriptor->field(i);

			switch(fd->label())
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

			string typeName;

			switch(fd->type())
			{
			case FieldDescriptor::TYPE_MESSAGE:
				typeName = MakeXLink(
					fd->message_type()->full_name(),
					fd->message_type()->name());
				break;
			case FieldDescriptor::TYPE_ENUM:
				typeName = MakeXLink(
					fd->enum_type()->full_name(),
					fd->enum_type()->name());
				break;
			default:
				typeName = fd->type_name();
				break;
			}

			WriteMessageInformalTableEntry(
				os, 
				fd->name(),
				typeName,
				labelStr,
				MakeDefaultValueString(fd),
				GetDescriptorComment(fd));
		}
	}

	void WriteEnumFieldEntries(
		std::ostringstream &os, 
		EnumDescriptor const *enumDescriptor)
	{
		for (int i = 0; i < enumDescriptor->value_count(); i++) {
			WriteEnumInformalTableEntry(
				os, 
				enumDescriptor->value(i)->name(),
				i,
				GetDescriptorComment(enumDescriptor->value(i)));
		}
	}

	template <typename DescriptorType>
	void WriteEnumTable( 
		DescriptorType const *descriptor, 
		std::ostringstream &os,
		string const &prefix,
		int section)
	{
		for (int i = 0; i < descriptor->enum_type_count(); i++) 
		{
			EnumDescriptor const *enumDescriptor = descriptor->enum_type(i);

			string xmlID = enumDescriptor->full_name();
			std::replace(xmlID.begin(), xmlID.end(),'.','_');

			string enumName;
			if(prefix.empty() == false)
			{
				enumName = prefix;
				enumName += ".";
				enumName += enumDescriptor->name();
			}
			else
			{
				enumName = enumDescriptor->name();
			}

			WriteEnumInformalTableHeader(
				os,
				xmlID, 
				enumName,
				GetDescriptorComment(enumDescriptor),
				section);

			WriteEnumFieldEntries(os, enumDescriptor);
			WriteInformalTableFooter(os, section);
		}
	}

	void WriteMessageTable(std::ostringstream &os, 
		Descriptor const *messageDescriptor, 
		string const &descriptorName,
		int sectionLevel)
	{
		// XML ID is an unique ID that is used in XLink. Since "." is not 
		// allowed, some sanitization is needed by replacing "." with "_".
		string xmlID = messageDescriptor->full_name();
		std::replace(xmlID.begin(), xmlID.end(),'.', '_');

		WriteMessageInformalTableHeader(
			os,
			xmlID, 
			descriptorName,
			GetDescriptorComment(messageDescriptor),
			sectionLevel);

		WriteMessageFieldEntries(os, messageDescriptor);

		WriteInformalTableFooter(os, sectionLevel);
	}

	//! @details
	//! Writes the message and recursively traverse its nested type into 
	//! the stream.
	//!
	//! @param[in,out] std::ostringstream & os
	//! Stream to write to.
	//!
	//! @param[in,out] Descriptor const * messageDescriptor
	//! The message descriptor that we are writing at the moment
	//!
	//! @param[in,out] string const & prefix
	//! The prefix that should be appended to the name.
	//!
	//! @param[in,out] int depth
	//! The depth of the recursion we are in.
	//!
	void WriteMessage(
		std::ostringstream &os, 
		Descriptor const *messageDescriptor, 
		string const &prefix, 
		int depth)
	{
		// Append the prefix to descriptor name so that the name will be
		// scoped descriptively. (E.g. name.child_type1.child_type2)
		string descriptorName;
		if(prefix.empty() == false)
		{
			descriptorName = prefix;
			descriptorName += ".";
			descriptorName += messageDescriptor->name();
		}
		else
		{
			descriptorName = messageDescriptor->name();
		}

		// Print this message with all of its field. This should generate a
		// InformalTable type in DocBook for this message.
		WriteMessageTable(os, messageDescriptor, descriptorName, depth+2);

		// Print the enums nested with this message. Since enum is defined 
		// within the message, its section level should be one below the 
		// parent message, hence +3.
		WriteEnumTable(messageDescriptor, os, descriptorName, depth+3);

		// Base case for the recursive call. If there is no nested type, 
		// then the formatting for this message is done.
		if(messageDescriptor->nested_type_count() == 0)
		{
			return;
		}
		// Otherwise, for each nested type, recursively print its own table.
		// Because of the recursive layout, the deepest layered message will
		// be printed last within its root message.
		else
		{
			for(int i=0; i<messageDescriptor->nested_type_count(); ++i)
			{
				WriteMessage(
					os, 
					messageDescriptor->nested_type(i), 
					descriptorName, 
					depth+1);
			}
		}
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

DocbookGenerator::DocbookGenerator()
{
	//! @remark
	//! Wait a bit to allow human being to attach the debugger to this process.
	//! Not sure if this is the best way to debug this type of interprocess
	//! program, but it works well enough for me. (askldjd)
	//Sleep(10000);

	// Upon construction, read the docbook.properties file once to load up
	// all the user options.
	s_docbookOptions = utils::ParseProperty("docbook.properties");
}

DocbookGenerator::~DocbookGenerator() 
{
}
bool DocbookGenerator::Generate(
	FileDescriptor const *file,
	string const &parameter,
	GeneratorContext *context,
	string *error) const 
{
	std::ostringstream os;
	WriteProtoFileHeader(os, file);

	for (int i = 0; i < file->message_type_count(); i++) 
	{
		WriteMessage(os, file->message_type(i), "", 0);
	}

	WriteEnumTable(file, os, "", 2);

	WriteProtoFileFooter(os);

	if(s_templateFileMade == false) 
	{
		s_templateFileMade = true;
		MakeTemplateFile(*context);
	}

	scoped_ptr<io::ZeroCopyOutputStream> output(
		context->OpenForInsert(DEFAULT_OUTPUT_NAME, DEFAULT_INSERTION_POINT));
	io::Printer printer(output.get(), '$');
	printer.PrintRaw(os.str().c_str());

	if (printer.failed()) {
		*error = "CodeGenerator detected write error.";
		return false;
	}

	return true;
}

}}}}  // end namespace
