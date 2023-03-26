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

#include "resources/XmlDocument.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include <algorithm>

namespace hpl {

	// //////////////////////////////////////////////////////////////////////////
	// // NODE
	// //////////////////////////////////////////////////////////////////////////

	// //-----------------------------------------------------------------------

	// IXMLNode::IXMLNode(eXmlNodeType aType, cXmlElement *apParent, const tString& asValue):
	// 	m_value(asValue),
	// 	m_parent(apParent)
	// {
	// }
	// //-----------------------------------------------------------------------

	// IXMLNode::~IXMLNode()
	// {
	// 	DestroyChildren();
	// }

	// cXmlElement* IXMLNode::GetFirstElement(const tString& asName)
	// {
	// 	return  FindChild(asName);
	// }

	// cXmlElement * IXMLNode::CreateChildElement(const tString& asName)
	// {
	// 	cXmlElement *pElement = hplNew(cXmlElement, (asName,this));

	// 	AddChild(pElement);

	// 	return pElement;
	// }

	// //-----------------------------------------------------------------------

	// void IXMLNode::AddChild(cXmlElement* apNode)
	// {
	// 	m_children.push_back(apNode);
	// }

	// void IXMLNode::DestroyChild(cXmlElement* apNode)
	// {
	// 	auto it = std::find_if(m_children.begin(), m_children.end(), [apNode](auto &p) { return p == apNode; });
	// 	if(it != m_children.end())
	// 	{
	// 		delete apNode;
	// 		m_children.erase(it);
	// 	}
	// }

	// cXmlElement* IXMLNode::FindChild(const std::string& name) {
	// 	for(auto& child: m_children) {
	// 		if(child->GetValue() == name) {
	// 			return child;
	// 		}
	// 	}
	// 	return nullptr;
	// }

	// cXmlElement::ChildCollection::iterator cXmlElement::Find(const std::string& name) {
	// 	return std::find_if(m_children.begin(), m_children.end(), [&name](auto &p) { return p.name() == name; });
	// }

	// //-----------------------------------------------------------------------

	// void IXMLNode::DestroyChildren()
	// {
	// 	for(auto& child: m_children) {
	// 		delete child;
	// 	}
	// }

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// ELEMENT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cXmlElement::cXmlElement() {
	}

	cXmlElement::~cXmlElement()
	{

	}
	//-----------------------------------------------------------------------

	const char* cXmlElement::GetAttribute(const tString& asName)
	{

		auto it = std::find_if(m_attributes.begin(), m_attributes.end(),[&](const auto& a) { return a.key() == asName;});
		if(it != m_attributes.end())
		{
			return it->value().c_str();
		}
		return nullptr;
	}

	//-----------------------------------------------------------------------


	XMLChild& cXmlElement::AddChild(const std::string_view& name) {
		return m_children.emplace_back(XMLChild(name, std::make_unique<cXmlElement>()));
	}

	XMLChild* cXmlElement::Find(const std::string& name) {
		auto it = std::find_if(m_children.begin(), m_children.end(), [&name](auto &p) { return p.name() == name; });
		if(it != m_children.end()) {
			return &*it;
		}
		return nullptr;
	}

	tString cXmlElement::GetAttributeString(const tString& asName, const tString& asDefault)
	{
		const char* pString = GetAttribute(asName);
		if(pString)	return pString;
		else		return asDefault;
	}

	float cXmlElement::GetAttributeFloat(const tString& asName, float afDefault)
	{
		const char* pString = GetAttribute(asName);
		return cString::ToFloat(pString,afDefault);
	}
	int cXmlElement::GetAttributeInt(const tString& asName, int alDefault)
	{
		const char* pString = GetAttribute(asName);
		return cString::ToInt(pString,alDefault);
	}
	bool cXmlElement::GetAttributeBool(const tString& asName, bool abDefault)
	{
		const char* pString = GetAttribute(asName);
		return cString::ToBool(pString,abDefault);
	}
	cVector2f cXmlElement::GetAttributeVector2f(const tString& asName, const cVector2f& avDefault)
	{
		const char* pString = GetAttribute(asName);
		return cString::ToVector2f(pString,avDefault);

	}
	cVector3f cXmlElement::GetAttributeVector3f(const tString& asName, const cVector3f& avDefault)
	{
		const char* pString = GetAttribute(asName);
		return cString::ToVector3f(pString,avDefault);
	}
	cColor cXmlElement::GetAttributeColor(const tString& asName, const cColor& aDefault)
	{
		const char* pString = GetAttribute(asName);
		return cString::ToColor(pString,aDefault);
	}

	//-----------------------------------------------------------------------

	void cXmlElement::SetAttribute(const tString& asName, const char* asVal)
	{
		auto it = std::find_if(m_attributes.begin(), m_attributes.end(),[&](const auto& a) { return a.key() == asName;});
		if(it != m_attributes.end())
		{
			it->m_value = asVal;
		}
		else
		{
			m_attributes.push_back({asName,asVal});
		}
	}

	//-----------------------------------------------------------------------

	static char gvTempStringBuffer[512];

	void cXmlElement::SetAttributeString(const tString& asName, const tString& asVal)
	{
		SetAttribute(asName, asVal.c_str());
	}
	void cXmlElement::SetAttributeFloat(const tString& asName, float afVal)
	{
		sprintf(gvTempStringBuffer, "%g", afVal);
		SetAttribute(asName, gvTempStringBuffer);
	}
	void cXmlElement::SetAttributeInt(const tString& asName, int alVal)
	{
		sprintf(gvTempStringBuffer, "%d", alVal);
		SetAttribute(asName, gvTempStringBuffer);
	}
	void cXmlElement::SetAttributeBool(const tString& asName, bool abVal)
	{
		SetAttribute(asName, abVal ? "true" : "false");
	}
	void cXmlElement::SetAttributeVector2f(const tString& asName, const cVector2f& avVal)
	{
		SetAttribute(asName,avVal.ToFileString().c_str());
	}
	void cXmlElement::SetAttributeVector3f(const tString& asName, const cVector3f& avVal)
	{
		SetAttribute(asName, avVal.ToFileString().c_str());
	}
	void cXmlElement::SetAttributeColor(const tString& asName, const cColor& aVal)
	{
		SetAttribute(asName, aVal.ToFileString().c_str());
	}

	iXmlDocument::iXmlDocument(const tString& asName):
		m_root(asName, std::make_unique<cXmlElement>()),
		msFile(_W(""))
	{
	}

	iXmlDocument::~iXmlDocument()
	{
	}

	bool iXmlDocument::CreateFromFile(const tWString& asPath)
	{
		bool bRet = LoadDataFromFile(asPath);
		if(bRet==false)
		{
			Log("Failed parsing of XML document %s in line %d, column %d: %s\n", cString::To8Char(asPath).c_str(),
																					mlErrorRow, mlErrorCol, msErrorDesc.c_str());
		}

		msFile = asPath;

		return bRet;
	}

	//-----------------------------------------------------------------------

	bool iXmlDocument::Save()
	{
		return SaveToFile(msFile);
	}

	bool iXmlDocument::SaveToFile(const tWString& asPath)
	{
		return SaveDataToFile(asPath);
	}

}
