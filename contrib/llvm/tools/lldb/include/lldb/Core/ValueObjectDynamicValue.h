//===-- ValueObjectDynamicValue.h -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ValueObjectDynamicValue_h_
#define liblldb_ValueObjectDynamicValue_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/ValueObject.h"
#include "lldb/Symbol/Type.h"

namespace lldb_private {

//----------------------------------------------------------------------
// A ValueObject that represents memory at a given address, viewed as some 
// set lldb type.
//----------------------------------------------------------------------
class ValueObjectDynamicValue : public ValueObject
{
public:
    ~ValueObjectDynamicValue() override;

    uint64_t
    GetByteSize() override;

    ConstString
    GetTypeName() override;

    ConstString
    GetQualifiedTypeName() override;

    ConstString
    GetDisplayTypeName() override;
    
    size_t
    CalculateNumChildren(uint32_t max) override;

    lldb::ValueType
    GetValueType() const override;

    bool
    IsInScope() override;
    
    bool
    IsDynamic() override
    {
        return true;
    }
    
    bool
    IsBaseClass () override
    {
        if (m_parent)
            return m_parent->IsBaseClass();
        return false;
    }
    
    bool
    GetIsConstant() const override
    {
        return false;
    }
    
    ValueObject *
    GetParent() override
    {
        return ((m_parent != nullptr) ? m_parent->GetParent() : nullptr);
    }

    const ValueObject *
    GetParent() const override
    {
        return ((m_parent != nullptr) ? m_parent->GetParent() : nullptr);
    }

    lldb::ValueObjectSP
    GetStaticValue() override
    {
        return m_parent->GetSP();
    }

    void
    SetOwningSP (lldb::ValueObjectSP &owning_sp)
    {
        if (m_owning_valobj_sp == owning_sp)
            return;
            
        assert (m_owning_valobj_sp.get() == nullptr);
        m_owning_valobj_sp = owning_sp;
    }
    
    bool
    SetValueFromCString(const char *value_str, Error& error) override;
    
    bool
    SetData(DataExtractor &data, Error &error) override;
    
    TypeImpl
    GetTypeImpl() override;
    
    lldb::VariableSP
    GetVariable () override
    {
        return m_parent ? m_parent->GetVariable() : nullptr;
    }
    
    lldb::LanguageType
    GetPreferredDisplayLanguage() override;
    
    void
    SetPreferredDisplayLanguage (lldb::LanguageType);
        
    bool
    IsSyntheticChildrenGenerated () override;
    
    void
    SetSyntheticChildrenGenerated (bool b) override;
    
    bool
    GetDeclaration(Declaration &decl) override;
    
    uint64_t
    GetLanguageFlags () override;
    
    void
    SetLanguageFlags (uint64_t flags) override;

protected:
    bool
    UpdateValue() override;
    
    LazyBool
    CanUpdateWithInvalidExecutionContext() override
    {
        return eLazyBoolYes;
    }
    
    lldb::DynamicValueType
    GetDynamicValueTypeImpl() override
    {
        return m_use_dynamic;
    }
    
    bool
    HasDynamicValueTypeInfo() override
    {
        return true;
    }
    
    CompilerType
    GetCompilerTypeImpl() override;

    Address  m_address;  ///< The variable that this value object is based upon
    TypeAndOrName m_dynamic_type_info; // We can have a type_sp or just a name
    lldb::ValueObjectSP m_owning_valobj_sp;
    lldb::DynamicValueType m_use_dynamic;
    TypeImpl m_type_impl;

private:
    friend class ValueObject;
    friend class ValueObjectConstResult;
    ValueObjectDynamicValue (ValueObject &parent, lldb::DynamicValueType use_dynamic);

    DISALLOW_COPY_AND_ASSIGN (ValueObjectDynamicValue);
};

} // namespace lldb_private

#endif // liblldb_ValueObjectDynamicValue_h_
