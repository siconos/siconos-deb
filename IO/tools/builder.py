#!/usr/bin/env python

# ./builder.py -I/usr/local/include/Siconos/Kernel \
#    -I/usr/local/include/Siconos/Numerics -I/usr/include/libxml2

# Mechanics
# ./builder.py -I/usr/local/include/Siconos/Kernel \
# -I/usr/local/include/Siconos/Mechanics \
# -I/usr/local/include/Siconos/Numerics -I/usr/include/libxml2 \
# --target=Mechanics

# we use pygccxml from Roman Yakovenko.
# http://sourceforge.net/projects/pygccxml/

import sys
import getopt
import re
import itertools
from pygccxml import parser
from pygccxml import declarations

myname = sys.argv[0]
include_paths = []
siconos_namespace = '::'


def generated_file(targ):
    return '../src/Siconos{0}Generated.hpp'.format(targ)


def usage():
    print '{0} [--namespace=<namespace>] -I<path> [-I<path> ...] \
               [--target=<target> ] header'.format(myname)


try:
    opts, args = getopt.getopt(sys.argv[1:], 'I:', ['help', 'namespace=',
                                                    'target='])
except getopt.GetoptError, err:
    print str(err)
    usage()
    sys.exit(2)

target = 'Kernel'

for opt, arg in opts:
    if opt == '--target':
        target = arg
    if opt == '-I':
        include_paths += [arg]
    if opt == '--namespace':
        siconos_namespace = arg
    if opt == '--help':
        usage()
        sys.exit(0)


if len(args) != 0:
    usage()
    sys.exit(1)

input_headers = dict()

input_headers['Kernel'] = ["SiconosKernel.hpp"]

input_headers['Mechanics'] = ["Circle.hpp",
                              "CircleCircleR.hpp", "CircularDS.hpp",
                              "Disk.hpp", "DiskDiskR.hpp",
                              "DiskMovingPlanR.hpp",
                              "DiskPlanR.hpp", "SphereLDS.hpp",
                              "SphereLDSPlanR.hpp",
                              "SphereLDSSphereLDSR.hpp", "SphereNEDS.hpp",
                              "SphereNEDSPlanR.hpp",
                              "SphereNEDSSphereNEDSR.hpp",
                              "SiconosBodies.hpp",
                              "SpaceFilter.hpp", "ExternalBody.hpp"]


config = parser.config_t(include_paths=include_paths)

decls = parser.parse(input_headers[target], config)
global_ns = declarations.get_global_namespace(decls)


def is_serializable(c):
    return 'serializable' in [c.name for c in c.typedefs(allow_empty=True)]


def unwanted(s):
    m = re.search('xml|XML|Xml|MBlockCSR|fPtr|SimpleMatrix|SimpleVector|SiconosVector|SiconosSet|DynamicalSystemsSet|SiconosGraph|SiconosSharedLibrary|numerics|computeFIntPtr|computeJacobianFIntqPtr|computeJacobianFIntqDotPtr|PrimalFrictionContact|FrictionContact|Lsodar|MLCP2|_moving_plans|_err', s)
    # note _err -> boost::circular_buffer issue with serialization
    return m != None


def name(t):
    if isinstance(t, declarations.class_t):
        return t.name
    elif isinstance(t, declarations.typedef_t):
        return t.decl_string[2:]  # remove ::

# classes in siconos_namespace
class_names = dict()

# class name of classes with a least a base (for the boost archive
# registration)
with_base = []


# a typedef table to replace templated class by their typedefs in
# macros call
typedef = dict()
for t in global_ns.typedefs():
    typedef[str(t._type)] = name(t)


def replace_by_typedef(some_type):
    if typedef.has_key(str(some_type)):
        rep_typedef = typedef[str(some_type)]
        if not '<' in rep_typedef:  # replace only if not a template
            return rep_typedef
    return str(some_type)

with open(generated_file(target), 'w') as dest_file:

    dest_file.write('// generated with the command : {0}\n'
                    .format(' '.join(sys.argv)))
    for header in input_headers[target]:
        dest_file.write('#include "{0}"\n'.format(header))

    for type_ in filter(lambda c: c.parent.name == siconos_namespace,
                        itertools.chain(
                            global_ns.classes(), global_ns.typedefs())):

        is_typedef = False

        if isinstance(type_, declarations.class_t):
            class_names[declarations.full_name(type_)] = type_
            class_ = type_
        elif isinstance(type_, declarations.typedef_t):
            try:
                is_typedef = True
                class_ = class_names['::' + str(type_.type)]
            except:
                class_ = None
        # with the serializabe tag
        # (could not find friend functions with pygccxml)
        if class_ is not None and \
            is_serializable(class_) and \
            (is_typedef or not
             declarations.templates.is_instantiation(class_.name)):

            if not unwanted(class_.name):

                if not class_.is_abstract:
                    with_base += [class_.name]

                # print registration macros depending on inheritance
                if class_.bases == []:
                    dest_file.write(
                        'SICONOS_IO_REGISTER({0},\n'.format(name(type_)))
                else:
                    serializable_bases = \
                        reduce(lambda r, b:
                               r + [b.related_class]
                               if is_serializable(b.related_class)
                               and
                               b.related_class.parent.name == siconos_namespace
                               else r, class_.bases, [])
                    if len(serializable_bases) > 0:
                        dest_file.write(
                        'SICONOS_IO_REGISTER_WITH_BASES({0},{1},\n'
                        .format(name(type_),
                              ''.join(['({0})'
                                       .format(replace_by_typedef(c.name))
                                       for c in serializable_bases])))
                    else:
                        dest_file.write('SICONOS_IO_REGISTER({0},\n'
                                        .format(name(type_)))

                variables = [v.name
                             for v in filter(lambda x: not 'void'
                                             in str(x._get_type()),
                                             class_.variables(
                                                 allow_empty=True))]

                dest_file.write('{0})\n'
                                .format('\n'
                                        .join('  ({0})'
                                              .format(vn)
                                              for vn in
                                              filter(lambda x: not unwanted(x),
                                                     variables))))

    # filtering is not correct at this point
    # some unwanted classes are necessary
    # (the ones in SiconosFull.hpp) others not (xml)
    # some leads to compilation errors.
    dest_file.write('\n')
    dest_file.write('template <class Archive>\n')
    dest_file.write('void siconos_io_register_{0}_generated(Archive& ar)\n'
                    .format(target))
    dest_file.write('{{\n{0}\n}}\n'
                    .format('\n'
                            .join(
                                '  ar.register_type(static_cast<{0}*>(NULL));'
                                .format(x) for x in with_base)))