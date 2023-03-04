#include <iostream>
#include <map>
#include <sstream>
#include <cassert>
#include <vector>
#include <type_traits>

#define SHOW(x) std::cout << "'" << #x << "' = " << (x) << "\n";
#define PRINT(x) std::cout << (#x) << "\n"; 

/**
* Reflection struct
*/

struct Reflection {
    using StrMap = std::map<std::string, std::string>;

    struct DefaultBase {
        inline static const std::string __Reflect_TypeName = "NOBASE";
        inline static std::map<std::string, void(*)(DefaultBase*, std::string&)> __Reflect_SetterFuncPtrs;
        inline static std::map<std::string, void(*)()>                           __Reflect_GetterFuncPtrs;
    };

    template <typename T>
    using IsNotBaseType = std::enable_if_t<!std::is_same_v<typename T::Base, Reflection::DefaultBase>, bool>;

    template <typename T>
    using IsBaseType = std::enable_if_t<std::is_same_v<typename T::Base, Reflection::DefaultBase>, bool>;
};



/* ------------------------------------------------------------------------------------------------ */

/**
* DUMP
*/
template<typename T, Reflection::IsNotBaseType<T> = false>
void DumpParentData(T& type, std::stringstream& ss)
{
    for(auto* field_getter : T::Base::__Reflect_GetterFuncPtrs)
    {
        ss << " " << field_getter(type) << ",";
    }
}

template<typename T, Reflection::IsBaseType<T> = false>
void DumpParentData(T& type, std::stringstream& ss)
{
    // doesnt do anything
    return;
}

template<typename T>
std::string Dump(T& type, bool include_header = true)
{
    std::stringstream ss;

    if(include_header)
    {
        ss << "{ ";
        if (type.__Reflect_InstanceName)
        {
            ss << "\""<< *type.__Reflect_InstanceName << "\" : ";
        }
        else
        {
            ss << " \"NONAME\" : ";
        }
        ss << type.__Reflect_TypeName << " =  {";
    }
    else
    {
        // hack to make deserialization of nested objects work with current parsing code
        ss << "{,";
    }
    
    DumpParentData(type, ss);
    for(auto* field_getter : type.__Reflect_GetterFuncPtrs)
    {
        ss << " " << field_getter(type) << ",";
    }
    ss << " } }\n";

    return ss.str();
}


/**
* SERIALIZATION TO_STRING
*/

// default template for serialization of fields
template<typename TField>
auto __Reflection_ToString(TField& field) -> decltype(std::to_string(field), std::string())
{
    return std::to_string(field);
};

// specialization for UserTypes (ReflectionTypes)
template<typename TField, std::enable_if_t<std::is_class_v<TField>, bool> = true>
auto __Reflection_ToString(TField& field) -> decltype(Dump<TField>(field), std::string())
{
    return Dump<TField>(field, false);
};

std::string __Reflection_ToString(std::string& field)
{
    if(field.empty())
        return "\"\"";

    return "\"" + field + "\"";
};

std::string __Reflection_ToString(char& field)
{
    return std::string(1, field);
};

std::string __Reflection_ToString(bool& field)
{
    if(field) return "true";
    return "false";
};

/* ------------------------------------------------------------------------------------------------ */

/**
* PARSE OBJECT
*/

void ParseObject(std::string& data, Reflection::StrMap& field_value_map, bool include_header)
{
    std::stringstream ss(data);
    std::string _;

    // discard header if parsing nested object
    getline(ss, _, '{');

    if(include_header)
    {
        getline(ss, _, '{');
    }

    std::string field_data;
    while(getline(ss, field_data, ','))
    {
        if(field_data == "")
            break;

        std::stringstream st(field_data);
        std::string field_name_token;
        std::string field_type_token;
        std::string field_value;

        getline(st, _, ' ');
        getline(st, field_name_token, ' ');

        // stop processing fields
        if(field_name_token[0] == '}')
            continue;

        getline(st, _, ' ');
        getline(st, field_type_token, ' ');
        getline(st, _, ' ');
        getline(st, field_value, ' ');

        // trim
        if(field_name_token.size() > 2)
        {
            field_name_token.erase(0, 1);
            field_name_token.erase(field_name_token.size() - 1);
        }

        // process nested object
        if(field_value[0] == '{')
        {
            std::string nested_object_data;
            std::stringstream ss_;

            getline(ss, nested_object_data, '}');

            ss_ << "{" << nested_object_data << "}";
            field_value = ss_.str();

        }

        field_value_map[field_name_token] = field_value;
    }
}

template<typename T, Reflection::IsNotBaseType<T> = true>
T Load(std::string& data, bool include_header = true)
{
    std::map<std::string, std::string> fields;
    ParseObject(data, fields, include_header);
    
	T instance;
	for(auto& [field, value] : fields)
	{
        auto it = T::__Reflect_SetterFuncPtrs.find(field);
        if(it == T::__Reflect_SetterFuncPtrs.end())  
        { 
            // Try finding setter func in parent class, then.
            auto& parent_setters = T::Base::__Reflect_SetterFuncPtrs;
            auto it_ = parent_setters.find(field);
            if(it_ != parent_setters.end())  
            {
                auto* setter_method = it_->second;
                setter_method(&instance, value);
            }
            else 
            { 
                SHOW("PROBLEM: Type have a base type but couldn't find the appropriate setter for parsed field.");
                SHOW(field);
                SHOW(value);
                assert(false);
            }
        }
        else
        {
            auto* setter_method = it->second;
            setter_method(&instance, value);
        }
    }

	return instance;
}

template<typename T, Reflection::IsBaseType<T> = true>
T Load(std::string& data, bool include_header = true)
{
    Reflection::StrMap fields;
    ParseObject(data, fields, include_header);
    
	T instance;
	for(auto& [field, value] : fields)
	{
        auto it = T::__Reflect_SetterFuncPtrs.find(field);
        if(it == T::__Reflect_SetterFuncPtrs.end())  
        { 
            SHOW("PROBLEM: Type doesn't have a base type and we couldn't find the appropriate setter for parsed field.");
            SHOW(field);
            SHOW(value);
            assert(false);
        }

        auto* setter_method = it->second;
        setter_method(&instance, value);
    }

	return instance;
}


/**
* PARSE VALUE FROM STRING
*/
template<typename T>
T __Reflection_FromString(std::string& value)
{
    return Load<T>(value, false);
};

template<>
int __Reflection_FromString<int>(std::string& value)
{
    return std::stoi(value);
};

template<>
float __Reflection_FromString<float>(std::string& value)
{
    return std::stof(value);
};

template<>
double __Reflection_FromString<double>(std::string& value)
{
    return std::stod(value);
};

template<>
long double __Reflection_FromString<long double>(std::string& value)
{
    return std::stold(value);
};

template<>
bool __Reflection_FromString<bool>(std::string& value)
{
    if(value == "true") return true;
    else return false;
};

template<>
char __Reflection_FromString<char>(std::string& value)
{
    return value[0];
};

template<>
std::string __Reflection_FromString<std::string>(std::string& value)
{
    if(value[0] == '"')
    {
        value.erase(0, 1);
        value.erase(value.size() - 1);
    }
    return value;
};

/* ------------------------------------------------------------------------------------------------ */


/**
 *  BEGIN TYPE MACRO

/**
 * SERIALIZABLE MACRO
 */

// Macro indirection to force TYPE_NAME resolution
#define _SERIALIZABLE(X) __SERIALIZABLE(X)
#define SERIALIZABLE _SERIALIZABLE(TYPE_NAME)

#define __SERIALIZABLE(Name) \
    using Self = Name; \
    inline static const std::string __Reflect_TypeName = #Name; \
    typedef std::string (*GetterFuncPtrType)(Name&); \
    inline static std::vector<GetterFuncPtrType> __Reflect_GetterFuncPtrs;\
    typedef void (*SetterFuncPtrType)(Name*, std::string&); \
    inline static std::map<std::string, SetterFuncPtrType> __Reflect_SetterFuncPtrs;\
    std::string* __Reflect_InstanceName = nullptr; \
    std::string __Reflect_SetName(std::string& name_var, std::string&& default_name){ __Reflect_InstanceName = &name_var; return default_name;} \
    std::string* __Reflect_GetInstanceName() { assert(("Type "#Name" doesn't define a field using the NAME() macro. Can't get instance name.", __Reflect_InstanceName)); return __Reflect_InstanceName; }; \

#define BASE_TYPE(TypeName) ; using Base = Reflection::DefaultBase ; _SERIALIZABLE(TypeName) ; 
#define DERIVED_TYPE(TypeName, Parent) ; using Base = Parent ; _SERIALIZABLE(TypeName) ;

/**
 * FIELD MACRO
 */

// Defines the getter and setter static functions for each field. Register these functions in getter vector / setter map using a helper static member
#define FIELD(Type, Name) ;  \
    inline static std::string __Reflect_Getter_##Type_##Name(Self& instance) { std::stringstream ss; ss << "'" << #Name << "' : " << #Type << " = " << __Reflection_ToString(instance.Name); return ss.str(); }; \
    inline static void __Reflect_Setter_##Type_##Name(Self* instance, std::string& value) { instance->Name = __Reflection_FromString<Type>(value); }; \
    inline static struct __Reflect_HelperType_##Type_##Name { \
        __Reflect_HelperType_##Type_##Name() { \
        __Reflect_GetterFuncPtrs.push_back(&Self::__Reflect_Getter_##Type_##Name); \
        __Reflect_SetterFuncPtrs[#Name] = &Self::__Reflect_Setter_##Type_##Name;\
        } } __discard_##Type_##Name{}; \
    Type Name
    

/**
 * NAME FIELD MACRO
 */
    
#define NAME(Name, Default); FIELD(std::string, Name) = __Reflect_SetName(Name, Default);


/* ------------------------------------------------------------------------------------------------ */

struct OtherThing
{
    BASE_TYPE(OtherThing);

private: 
    FIELD(float, x) = 0.f;
    
    FIELD(float, y) = 0.f;
    
    FIELD(float, z) = 0.f;

    int not_serialized_int = 34;
};


/* Inheritance support testing */
struct Parent : public OtherThing
{
    BASE_TYPE(Parent);

    NAME(parent_name, "DefaultParentName");

    FIELD(float, x) = 0.f;
};


struct Child : Parent
{
    DERIVED_TYPE(Child, Parent)

    NAME(child_name, "");

    FIELD(float, a) = 0.f;
};


struct NotSerializableThing
{
    int x, y, z;
};

struct Thing
{
    BASE_TYPE(Thing);

    FIELD(int, id);

    NAME(name, "DefaultName");

    FIELD(float, value);
    
    FIELD(bool, flag) = false;

    FIELD(char, letter) = 0;

    FIELD(double, dValue) = 0.0;

    FIELD(OtherThing, other);

//  REF(OtherThing*, otherthing_ref);

protected:
    NotSerializableThing nst;
};

/*
    Problems to solve with deserialization:

    1. WHERE are entities stored? It should be flexible, we must be able to write CUSTOM CODE that can get a memory
    location an use it to be the place where that newly created object will live.

    2. 

/*



/*
    - DECLARE_DESERIALIZATION_FUNC(FNAME);

    void FNAME##_Internal(Reflection::LoadedInstances Instances);

    inline void LoadObjectsFromFile()
    {
        // a map of hash (long unsigned int?) to void*
        Reflection::LoadedInstances __Hashed_Objects;
        FNAME##_Internal(__Hashed_Objects);

        // resolve late refs with lambda callbacks that are invoked?
    }
*/



/*
    POINTERS!

    we could store a hash of the instance on their other end. we could store a UUID for the resource they point to.


    1. Create hash function for UserTypes, don't use REF fields, only FIELDS (maybe fieldnames and fieldvalues together)

    2. Once a REF is found, we can either resolve it at that time or later, in case the ref object wasn't loaded yet.

    3. 

*/


int main()
{
    std::cout << "\n(DE)SERIALIZATION EXAMPLES\n";
    std::string serialized_OtherThing = "{ \"OtherThing1\" : OtherThing { 'x' : float = 32.5, 'y' : float = 12.0, } }";
    std::string serialized_Thing = "{ \"Thing1\" : Thing { 'id' : int = 2, 'name' : std::string = \"Thing1\", 'value' : float = 55.5, 'flag' : bool = true, 'letter' : char = B, 'other' : OtherThing = {, 'x' : float = 12.0, 'y' : float = 22.0, 'z' : float = 33.0, }, } }";
    
    // Parse a simple object
    OtherThing ot = Load<OtherThing>(serialized_OtherThing);
    auto dumped_ot = Dump(ot);
    SHOW(dumped_ot);

    // Parse an object with another serializable object in it, then serialize it again
    Thing thing = Load<Thing>(serialized_Thing);
    std::string serialized_thing = Dump(thing);
    SHOW(serialized_thing);

    // Parse the dumped version of the same object and serialize it again
    Thing reloaded_thing = Load<Thing>(serialized_thing);
    std::string dumped_reloaded_thing = Dump(reloaded_thing);
    SHOW(dumped_reloaded_thing);
    // Assert that both serializations rounds match
    assert(dumped_reloaded_thing == serialized_thing);

    // Parse and dump an object that inherits from a serializable parent class
    std::string serialized_child = "{ \"Child1\" : Child { 'x' : float = 0.2, 'child_name' : std::string = \"Child1\", 'a' : float = 0.5, } }";
    Child child = Load<Child>(serialized_child);
    auto dumped_child = Dump(child);
    SHOW(dumped_child); 

    return 0;
}