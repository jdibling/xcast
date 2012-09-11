require 'tempfile'
require 'fileutils'

# usage:  'build_inc.rb filename'

if ARGV.size != 1
  puts "Usage: 'build_inc.rb filename'"
  exit 1
end

if !File.exists?(ARGV[0])
  puts "Error: File Does Not Exist"
  puts "\t#{ARGV[0]}"
  return 2
end

in_file = File.open(ARGV[0], "r")
out_file = Tempfile.new('version.h')

prev_build = nil
new_build = nil

rx = /(\s?#define\s+VERSION_BUILD\s+)(\d+)/

in_file.each {|line|
  m = line.match rx
  if m.nil?
    out_file.puts line
  else
    prev_build = $2.to_i
    new_build = prev_build + 1
    out_file.puts "#{$1}#{new_build}"
  end
}

if prev_build.nil? || new_build.nil?
  puts "Couldn't Find #{rx.to_s}"
  exit 3
end

puts "Incremented Build # from #{prev_build} to #{new_build}"

in_file.close
File.rename ARGV[0], "#{ARGV[0]}-#{prev_build}"

out_file.close
FileUtils.copy out_file.path.to_s, ARGV[0]



