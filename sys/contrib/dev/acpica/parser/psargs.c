/******************************************************************************
 *
 * Module Name: psargs - Parse AML opcode arguments
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#define __PSARGS_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdispat.h>

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psargs")

/* Local prototypes */

static UINT32
AcpiPsGetNextPackageLength (
    ACPI_PARSE_STATE        *ParserState);

static ACPI_PARSE_OBJECT *
AcpiPsGetNextField (
    ACPI_PARSE_STATE        *ParserState);


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextPackageLength
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Decoded package length. On completion, the AML pointer points
 *              past the length byte or bytes.
 *
 * DESCRIPTION: Decode and return a package length field.
 *              Note: Largest package length is 28 bits, from ACPI specification
 *
 ******************************************************************************/

static UINT32
AcpiPsGetNextPackageLength (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Aml = ParserState->Aml;
    UINT32                  PackageLength = 0;
    UINT32                  ByteCount;
    UINT8                   ByteZeroMask = 0x3F; /* Default [0:5] */


    ACPI_FUNCTION_TRACE (PsGetNextPackageLength);


    /*
     * Byte 0 bits [6:7] contain the number of additional bytes
     * used to encode the package length, either 0,1,2, or 3
     */
    ByteCount = (Aml[0] >> 6);
    ParserState->Aml += ((ACPI_SIZE) ByteCount + 1);

    /* Get bytes 3, 2, 1 as needed */

    while (ByteCount)
    {
        /*
         * Final bit positions for the package length bytes:
         *      Byte3->[20:27]
         *      Byte2->[12:19]
         *      Byte1->[04:11]
         *      Byte0->[00:03]
         */
        PackageLength |= (Aml[ByteCount] << ((ByteCount << 3) - 4));

        ByteZeroMask = 0x0F; /* Use bits [0:3] of byte 0 */
        ByteCount--;
    }

    /* Byte 0 is a special case, either bits [0:3] or [0:5] are used */

    PackageLength |= (Aml[0] & ByteZeroMask);
    return_UINT32 (PackageLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextPackageEnd
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Pointer to end-of-package +1
 *
 * DESCRIPTION: Get next package length and return a pointer past the end of
 *              the package.  Consumes the package length field
 *
 ******************************************************************************/

UINT8 *
AcpiPsGetNextPackageEnd (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Start = ParserState->Aml;
    UINT32                  PackageLength;


    ACPI_FUNCTION_TRACE (PsGetNextPackageEnd);


    /* Function below updates ParserState->Aml */

    PackageLength = AcpiPsGetNextPackageLength (ParserState);

    return_PTR (Start + PackageLength); /* end of package */
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextNamestring
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Pointer to the start of the name string (pointer points into
 *              the AML.
 *
 * DESCRIPTION: Get next raw namestring within the AML stream.  Handles all name
 *              prefix characters.  Set parser state to point past the string.
 *              (Name is consumed from the AML.)
 *
 ******************************************************************************/

char *
AcpiPsGetNextNamestring (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Start = ParserState->Aml;
    UINT8                   *End = ParserState->Aml;


    ACPI_FUNCTION_TRACE (PsGetNextNamestring);


    /* Point past any namestring prefix characters (backslash or carat) */

    while (AcpiPsIsPrefixChar (*End))
    {
        End++;
    }

    /* Decode the path prefix character */

    switch (*End)
    {
    case 0:

        /* NullName */

        if (End == Start)
        {
            Start = NULL;
        }
        End++;
        break;

    case AML_DUAL_NAME_PREFIX:

        /* Two name segments */

        End += 1 + (2 * ACPI_NAME_SIZE);
        break;

    case AML_MULTI_NAME_PREFIX_OP:

        /* Multiple name segments, 4 chars each, count in next byte */

        End += 2 + (*(End + 1) * ACPI_NAME_SIZE);
        break;

    default:

        /* Single name segment */

        End += ACPI_NAME_SIZE;
        break;
    }

    ParserState->Aml = End;
    return_PTR ((char *) Start);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextNamepath
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              Arg                 - Where the namepath will be stored
 *              ArgCount            - If the namepath points to a control method
 *                                    the method's argument is returned here.
 *              PossibleMethodCall  - Whether the namepath can possibly be the
 *                                    start of a method call
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get next name (if method call, return # of required args).
 *              Names are looked up in the internal namespace to determine
 *              if the name represents a control method.  If a method
 *              is found, the number of arguments to the method is returned.
 *              This information is critical for parsing to continue correctly.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsGetNextNamepath (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_STATE        *ParserState,
    ACPI_PARSE_OBJECT       *Arg,
    BOOLEAN                 PossibleMethodCall)
{
    ACPI_STATUS             Status;
    char                    *Path;
    ACPI_PARSE_OBJECT       *NameOp;
    ACPI_OPERAND_OBJECT     *MethodDesc;
    ACPI_NAMESPACE_NODE     *Node;
    UINT8                   *Start = ParserState->Aml;


    ACPI_FUNCTION_TRACE (PsGetNextNamepath);


    Path = AcpiPsGetNextNamestring (ParserState);
    AcpiPsInitOp (Arg, AML_INT_NAMEPATH_OP);

    /* Null path case is allowed, just exit */

    if (!Path)
    {
        Arg->Common.Value.Name = Path;
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Lookup the name in the internal namespace, starting with the current
     * scope. We don't want to add anything new to the namespace here,
     * however, so we use MODE_EXECUTE.
     * Allow searching of the parent tree, but don't open a new scope -
     * we just want to lookup the object (must be mode EXECUTE to perform
     * the upsearch)
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Path,
                ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE,
                ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE, NULL, &Node);

    /*
     * If this name is a control method invocation, we must
     * setup the method call
     */
    if (ACPI_SUCCESS (Status) &&
        PossibleMethodCall &&
        (Node->Type == ACPI_TYPE_METHOD))
    {
        if (WalkState->Opcode == AML_UNLOAD_OP)
        {
            /*
             * AcpiPsGetNextNamestring has increased the AML pointer,
             * so we need to restore the saved AML pointer for method call.
             */
            WalkState->ParserState.Aml = Start;
            WalkState->ArgCount = 1;
            AcpiPsInitOp (Arg, AML_INT_METHODCALL_OP);
            return_ACPI_STATUS (AE_OK);
        }

        /* This name is actually a control method invocation */

        MethodDesc = AcpiNsGetAttachedObject (Node);
        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
            "Control Method - %p Desc %p Path=%p\n", Node, MethodDesc, Path));

        NameOp = AcpiPsAllocOp (AML_INT_NAMEPATH_OP);
        if (!NameOp)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Change Arg into a METHOD CALL and attach name to it */

        AcpiPsInitOp (Arg, AML_INT_METHODCALL_OP);
        NameOp->Common.Value.Name = Path;

        /* Point METHODCALL/NAME to the METHOD Node */

        NameOp->Common.Node = Node;
        AcpiPsAppendArg (Arg, NameOp);

        if (!MethodDesc)
        {
            ACPI_ERROR ((AE_INFO,
                "Control Method %p has no attached object",
                Node));
            return_ACPI_STATUS (AE_AML_INTERNAL);
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
            "Control Method - %p Args %X\n",
            Node, MethodDesc->Method.ParamCount));

        /* Get the number of arguments to expect */

        WalkState->ArgCount = MethodDesc->Method.ParamCount;
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Special handling if the name was not found during the lookup -
     * some NotFound cases are allowed
     */
    if (Status == AE_NOT_FOUND)
    {
        /* 1) NotFound is ok during load pass 1/2 (allow forward references) */

        if ((WalkState->ParseFlags & ACPI_PARSE_MODE_MASK) !=
                ACPI_PARSE_EXECUTE)
        {
            Status = AE_OK;
        }

        /* 2) NotFound during a CondRefOf(x) is ok by definition */

        else if (WalkState->Op->Common.AmlOpcode == AML_COND_REF_OF_OP)
        {
            Status = AE_OK;
        }

        /*
         * 3) NotFound while building a Package is ok at this point, we
         * may flag as an error later if slack mode is not enabled.
         * (Some ASL code depends on allowing this behavior)
         */
        else if ((Arg->Common.Parent) &&
            ((Arg->Common.Parent->Common.AmlOpcode == AML_PACKAGE_OP) ||
             (Arg->Common.Parent->Common.AmlOpcode == AML_VAR_PACKAGE_OP)))
        {
            Status = AE_OK;
        }
    }

    /* Final exception check (may have been changed from code above) */

    if (ACPI_FAILURE (Status))
    {
        ACPI_ERROR_NAMESPACE (Path, Status);

        if ((WalkState->ParseFlags & ACPI_PARSE_MODE_MASK) ==
                ACPI_PARSE_EXECUTE)
        {
            /* Report a control method execution error */

            Status = AcpiDsMethodError (Status, WalkState);
        }
    }

    /* Save the namepath */

    Arg->Common.Value.Name = Path;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextSimpleArg
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *              ArgType             - The argument type (AML_*_ARG)
 *              Arg                 - Where the argument is returned
 *
 * RETURN:      None
 *
 * DESCRIPTION: Get the next simple argument (constant, string, or namestring)
 *
 ******************************************************************************/

void
AcpiPsGetNextSimpleArg (
    ACPI_PARSE_STATE        *ParserState,
    UINT32                  ArgType,
    ACPI_PARSE_OBJECT       *Arg)
{
    UINT32                  Length;
    UINT16                  Opcode;
    UINT8                   *Aml = ParserState->Aml;


    ACPI_FUNCTION_TRACE_U32 (PsGetNextSimpleArg, ArgType);


    switch (ArgType)
    {
    case ARGP_BYTEDATA:

        /* Get 1 byte from the AML stream */

        Opcode = AML_BYTE_OP;
        Arg->Common.Value.Integer = (UINT64) *Aml;
        Length = 1;
        break;


    case ARGP_WORDDATA:

        /* Get 2 bytes from the AML stream */

        Opcode = AML_WORD_OP;
        ACPI_MOVE_16_TO_64 (&Arg->Common.Value.Integer, Aml);
        Length = 2;
        break;


    case ARGP_DWORDDATA:

        /* Get 4 bytes from the AML stream */

        Opcode = AML_DWORD_OP;
        ACPI_MOVE_32_TO_64 (&Arg->Common.Value.Integer, Aml);
        Length = 4;
        break;


    case ARGP_QWORDDATA:

        /* Get 8 bytes from the AML stream */

        Opcode = AML_QWORD_OP;
        ACPI_MOVE_64_TO_64 (&Arg->Common.Value.Integer, Aml);
        Length = 8;
        break;


    case ARGP_CHARLIST:

        /* Get a pointer to the string, point past the string */

        Opcode = AML_STRING_OP;
        Arg->Common.Value.String = ACPI_CAST_PTR (char, Aml);

        /* Find the null terminator */

        Length = 0;
        while (Aml[Length])
        {
            Length++;
        }
        Length++;
        break;


    case ARGP_NAME:
    case ARGP_NAMESTRING:

        AcpiPsInitOp (Arg, AML_INT_NAMEPATH_OP);
        Arg->Common.Value.Name = AcpiPsGetNextNamestring (ParserState);
        return_VOID;


    default:

        ACPI_ERROR ((AE_INFO, "Invalid ArgType %X", ArgType));
        return_VOID;
    }

    AcpiPsInitOp (Arg, Opcode);
    ParserState->Aml += Length;
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextField
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      A newly allocated FIELD op
 *
 * DESCRIPTION: Get next field (NamedField, ReservedField, or AccessField)
 *
 ******************************************************************************/

static ACPI_PARSE_OBJECT *
AcpiPsGetNextField (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT32                  AmlOffset = (UINT32)
                                ACPI_PTR_DIFF (ParserState->Aml,
                                               ParserState->AmlStart);
    ACPI_PARSE_OBJECT       *Field;
    UINT16                  Opcode;
    UINT32                  Name;


    ACPI_FUNCTION_TRACE (PsGetNextField);


    /* Determine field type */

    switch (ACPI_GET8 (ParserState->Aml))
    {
    default:

        Opcode = AML_INT_NAMEDFIELD_OP;
        break;

    case 0x00:

        Opcode = AML_INT_RESERVEDFIELD_OP;
        ParserState->Aml++;
        break;

    case 0x01:

        Opcode = AML_INT_ACCESSFIELD_OP;
        ParserState->Aml++;
        break;
    }

    /* Allocate a new field op */

    Field = AcpiPsAllocOp (Opcode);
    if (!Field)
    {
        return_PTR (NULL);
    }

    Field->Common.AmlOffset = AmlOffset;

    /* Decode the field type */

    switch (Opcode)
    {
    case AML_INT_NAMEDFIELD_OP:

        /* Get the 4-character name */

        ACPI_MOVE_32_TO_32 (&Name, ParserState->Aml);
        AcpiPsSetName (Field, Name);
        ParserState->Aml += ACPI_NAME_SIZE;

        /* Get the length which is encoded as a package length */

        Field->Common.Value.Size = AcpiPsGetNextPackageLength (ParserState);
        break;


    case AML_INT_RESERVEDFIELD_OP:

        /* Get the length which is encoded as a package length */

        Field->Common.Value.Size = AcpiPsGetNextPackageLength (ParserState);
        break;


    case AML_INT_ACCESSFIELD_OP:

        /*
         * Get AccessType and AccessAttrib and merge into the field Op
         * AccessType is first operand, AccessAttribute is second
         */
        Field->Common.Value.Integer = (((UINT32) ACPI_GET8 (ParserState->Aml) << 8));
        ParserState->Aml++;
        Field->Common.Value.Integer |= ACPI_GET8 (ParserState->Aml);
        ParserState->Aml++;
        break;

    default:

        /* Opcode was set in previous switch */
        break;
    }

    return_PTR (Field);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetNextArg
 *
 * PARAMETERS:  WalkState           - Current state
 *              ParserState         - Current parser state object
 *              ArgType             - The argument type (AML_*_ARG)
 *              ReturnArg           - Where the next arg is returned
 *
 * RETURN:      Status, and an op object containing the next argument.
 *
 * DESCRIPTION: Get next argument (including complex list arguments that require
 *              pushing the parser stack)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsGetNextArg (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_STATE        *ParserState,
    UINT32                  ArgType,
    ACPI_PARSE_OBJECT       **ReturnArg)
{
    ACPI_PARSE_OBJECT       *Arg = NULL;
    ACPI_PARSE_OBJECT       *Prev = NULL;
    ACPI_PARSE_OBJECT       *Field;
    UINT32                  Subop;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR (PsGetNextArg, ParserState);


    switch (ArgType)
    {
    case ARGP_BYTEDATA:
    case ARGP_WORDDATA:
    case ARGP_DWORDDATA:
    case ARGP_CHARLIST:
    case ARGP_NAME:
    case ARGP_NAMESTRING:

        /* Constants, strings, and namestrings are all the same size */

        Arg = AcpiPsAllocOp (AML_BYTE_OP);
        if (!Arg)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }
        AcpiPsGetNextSimpleArg (ParserState, ArgType, Arg);
        break;


    case ARGP_PKGLENGTH:

        /* Package length, nothing returned */

        ParserState->PkgEnd = AcpiPsGetNextPackageEnd (ParserState);
        break;


    case ARGP_FIELDLIST:

        if (ParserState->Aml < ParserState->PkgEnd)
        {
            /* Non-empty list */

            while (ParserState->Aml < ParserState->PkgEnd)
            {
                Field = AcpiPsGetNextField (ParserState);
                if (!Field)
                {
                    return_ACPI_STATUS (AE_NO_MEMORY);
                }

                if (Prev)
                {
                    Prev->Common.Next = Field;
                }
                else
                {
                    Arg = Field;
                }
                Prev = Field;
            }

            /* Skip to End of byte data */

            ParserState->Aml = ParserState->PkgEnd;
        }
        break;


    case ARGP_BYTELIST:

        if (ParserState->Aml < ParserState->PkgEnd)
        {
            /* Non-empty list */

            Arg = AcpiPsAllocOp (AML_INT_BYTELIST_OP);
            if (!Arg)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            /* Fill in bytelist data */

            Arg->Common.Value.Size = (UINT32)
                ACPI_PTR_DIFF (ParserState->PkgEnd, ParserState->Aml);
            Arg->Named.Data = ParserState->Aml;

            /* Skip to End of byte data */

            ParserState->Aml = ParserState->PkgEnd;
        }
        break;


    case ARGP_TARGET:
    case ARGP_SUPERNAME:
    case ARGP_SIMPLENAME:

        Subop = AcpiPsPeekOpcode (ParserState);
        if (Subop == 0                  ||
            AcpiPsIsLeadingChar (Subop) ||
            AcpiPsIsPrefixChar (Subop))
        {
            /* NullName or NameString */

            Arg = AcpiPsAllocOp (AML_INT_NAMEPATH_OP);
            if (!Arg)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            /* To support SuperName arg of Unload */

            if (WalkState->Opcode == AML_UNLOAD_OP)
            {
                Status = AcpiPsGetNextNamepath (WalkState, ParserState, Arg, 1);

                /*
                 * If the SuperName arg of Unload is a method call,
                 * we have restored the AML pointer, just free this Arg
                 */
                if (Arg->Common.AmlOpcode == AML_INT_METHODCALL_OP)
                {
                    AcpiPsFreeOp (Arg);
                    Arg = NULL;
                }
            }
            else
            {
                Status = AcpiPsGetNextNamepath (WalkState, ParserState, Arg, 0);
            }
        }
        else
        {
            /* Single complex argument, nothing returned */

            WalkState->ArgCount = 1;
        }
        break;


    case ARGP_DATAOBJ:
    case ARGP_TERMARG:

        /* Single complex argument, nothing returned */

        WalkState->ArgCount = 1;
        break;


    case ARGP_DATAOBJLIST:
    case ARGP_TERMLIST:
    case ARGP_OBJLIST:

        if (ParserState->Aml < ParserState->PkgEnd)
        {
            /* Non-empty list of variable arguments, nothing returned */

            WalkState->ArgCount = ACPI_VAR_ARGS;
        }
        break;


    default:

        ACPI_ERROR ((AE_INFO, "Invalid ArgType: %X", ArgType));
        Status = AE_AML_OPERAND_TYPE;
        break;
    }

    *ReturnArg = Arg;
    return_ACPI_STATUS (Status);
}
