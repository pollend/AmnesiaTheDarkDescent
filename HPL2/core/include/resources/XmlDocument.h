/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_XML_DOCUMENT_H
#define HPL_XML_DOCUMENT_H

#include "absl/container/inlined_vector.h"
#include "math/Crc32.h"
#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "engine/RTTI.h"

#include <memory>
#include <span>
#include <variant>


namespace hpl {

	//-------------------------------------

	enum eXmlNodeType
	{
		eXmlNodeType_Element,

		eXmlNodeType_LastEnum
	};

	//-------------------------------------

	class IXMLNode;
	class cXmlElement;

	class XMLAttribute {
	public:
		XMLAttribute(const std::string& key, const std::string& value) : m_key(key), m_value(value) {}
		const std::string& key() const { return m_key; }
		const std::string& value() const { return m_value; }
	private:
		std::string m_key;
		std::string m_value;

		friend class cXmlElement;
	};

	class XMLChild final {
	public:
		XMLChild(const std::string_view& name, std::unique_ptr<cXmlElement>&& node) : 
			m_name(name), 
			m_node(std::move(node)) {}
		const std::string& name() const { return m_name; }
		inline cXmlElement* Content() { return m_node.get(); }
		void setName(const std::string_view name) { m_name = name; }
	private:
		std::string m_name;
		std::unique_ptr<cXmlElement> m_node;
	};

	class cXmlElement final
	{
		HPL_RTTI_CLASS(cXmlElement, "{46e8168a-271d-41bc-b109-01fa325d0c1a}")
	public:
		using AttributeCollection = absl::InlinedVector<XMLAttribute, 4>;
		using ChildCollection = absl::InlinedVector<XMLChild, 4>;

		cXmlElement();
		~cXmlElement();

		const char* GetAttribute(const tString& asName);

		tString GetAttributeString(const tString& asName, const tString& asDefault="");
		float GetAttributeFloat(const tString& asName, float afDefault=0);
		int GetAttributeInt(const tString& asName, int alDefault=0);
		bool GetAttributeBool(const tString& asName, bool abDefault=false);
		cVector2f GetAttributeVector2f(const tString& asName, const cVector2f& avDefault=0);
		cVector3f GetAttributeVector3f(const tString& asName, const cVector3f& avDefault=0);
		cColor GetAttributeColor(const tString& asName, const cColor& aDefault=cColor(0,0));

		void SetAttribute(const tString& asName, const char* asVal);

		void SetAttributeString(const tString& asName, const tString& asVal);
		void SetAttributeFloat(const tString& asName, float afVal);
		void SetAttributeInt(const tString& asName, int alVal);
		void SetAttributeBool(const tString& asName, bool abVal);
		void SetAttributeVector2f(const tString& asName, const cVector2f& avVal);
		void SetAttributeVector3f(const tString& asName, const cVector3f& avVal);
		void SetAttributeColor(const tString& asName, const cColor& aVal);

		XMLChild& AddChild(const std::string_view& name = "");
		XMLChild* Find(const std::string& name);
		inline ChildCollection& Children() { return m_children; }
		inline AttributeCollection& Attributes() { return m_attributes; }
	private:
		AttributeCollection m_attributes;
		ChildCollection m_children;
	};

	//-------------------------------------

	class iXmlDocument
	{
		HPL_RTTI_CLASS(iXmlDocument, "{46e8168a-271d-41bc-b109-01fa325d0c1a}")
	public:
		iXmlDocument(const tString& asName);
		virtual ~iXmlDocument();

		void SetPath(const tWString& asPath) { msFile = asPath; }
		const tWString& GetPath() { return msFile; }

		bool CreateFromFile(const tWString& asPath);
		bool Save();
		bool SaveToFile(const tWString& asPath);

		const tString& GetErrorDesc() { return msErrorDesc; }
		int GetErrorRow() { return mlErrorRow; }
		int GetErrorCol() { return mlErrorCol; }

		virtual void SaveToString(tString *apDestData)=0;
		virtual bool CreateFromString(const tString& asData)=0;

		XMLChild& Root() { return m_root; }

	protected:
		void SaveErrorInfo(const tString& asDesc, int alRow, int alCol) { msErrorDesc = asDesc; mlErrorRow = alRow; mlErrorCol = alCol; }

	private:
		virtual bool LoadDataFromFile(const tWString& asPath)=0;
		virtual bool SaveDataToFile(const tWString& asPath)=0;
		
		XMLChild m_root;
		tWString msFile;
		tString msErrorDesc;
		int		mlErrorRow;
		int		mlErrorCol;
	};

};
#endif // HPL_XML_DOCUMENT_H
