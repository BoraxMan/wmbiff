#!/usr/bin/ruby

# some OSes are finicky about the order of included header
# files.  this script looks through the .c files in wmbiff
# to verify that some headers (like sys/types) are included
# before others.

$failed = 0
FILES = 'wmgeneral/*.[ch] wmbiff/*.[ch]'

def include_a_before_b(first, second) 
    typeincl = Hash.new(9000)
    IO.popen('egrep -n "include *[<\"]' + first + '[>.]" ' + FILES, 'r').readlines.each { |l|
        file,line,text = l.split(':');
        typeincl[file] = line;
    }
    
    IO.popen('grep -n "include *[<\"]' + second + '[>.]" ' + FILES, 'r').readlines.each { |l|
        file,line,text = l.split(':');
        if(typeincl[file].to_i > line.to_i) then
            $failed = 1;
            if( typeincl[file].to_i < 9000 ) then
              puts "#{file}:#{typeincl[file].to_i} doesn't include #{first} (line #{typeincl[file].to_i}) before #{second}"
            else
              puts "#{file}:#{line.to_i} doesn't include #{first} though it includes #{second}"
            end
        end
    }
end

def unportable_function(fn)
  typeincl = Hash.new(9000)
  IO.popen("egrep -n \"#{fn} *\\(\" " + FILES, 'r').readlines.each { |l|
    file,line,text = l.split(':');
    typeincl[file] = line;
    $failed = 1
  }
  typeincl.each { |f,l| 
    puts "#{f}:#{l} references unportable function #{fn}"
  }
end

def unportable_constant(cn)
  typeincl = Hash.new(9000)
  IO.popen("egrep -n #{cn} " + FILES, 'r').readlines.each { |l|
    file,line,text = l.split(':');
    typeincl[file] = line;
    $failed = 1
  }
  typeincl.each { |f,l| 
    puts "#{f}:#{l} references unportable constant #{cn}"
  }
end
  

include_a_before_b("sys/types", "sys/socket") 
include_a_before_b("sys/types", "fcntl") 
include_a_before_b("sys/types", "dirent") # mac
include_a_before_b("sys/types", "netinet/in") # mac
include_a_before_b("sys/types", "netinet/in_systm") 
include_a_before_b("netinet/in_systm", "netinet/ip") 
include_a_before_b("netinet/in", "netinet/ip") 
include_a_before_b("netinet/in", "netinet/if_ether") # bsd
include_a_before_b("sys/socket", "netinet/if_ether") # bsd
include_a_before_b("sys/types", "netinet/if_ether") # bsd
include_a_before_b("sys/types", "sys/resource")  # not sure if there's more 
include_a_before_b("sys/time", "sys/resource")   # order here.
# also, nscommon should follow system header files like dirent, stdlib.

unportable_function("bcopy"); # not present on solaris
unportable_function("bzero"); # would rather just use memcpy, memset
unportable_function("asprintf"); # easy enough to malloc(strlen())
unportable_constant("ICMP_TIMESTAMP"); # use TSTAMP instead.
unportable_constant("uint64_t"); # use u_int64_t instead.
unportable_constant("uint32_t"); # use u_int32_t instead.

Kernel.exit($failed)
