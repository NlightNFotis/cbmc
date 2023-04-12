#!/usr/bin/env ruby

require 'uri'

if ARGV.length < 2
    puts "make_release.rb: Change version numbers of various CBMC subcomponents and push a release PR."
    puts
    puts "Usage: ./make_release.rb <old_version> <new_version>"
    puts "Example: ./make_release.rb 5.80.0 5.81.0"
    exit
end

old_version = ARGV[0]
new_version = ARGV[1]
remote = 'fotis'

file_names = ['../src/config.inc', '../src/libcprover-rust/Cargo.toml']

# Create new release branch (ignore any output from stdout)
`git checkout -b new_cbmc_release-#{new_version}`

file_names.each do |file_name|
  # Open file
  text = File.read(file_name)
  # Substitute new version
  new_contents = text.gsub(old_version, new_version)
  # Write version to file
  File.open(file_name, "w") { |file| file.puts new_contents }
  # Add file to git index
  `git add #{file_name}`
end

# Perform commit
output = `git commit -m "Mark CBMC cbmc-#{new_version}."`
if $?.success?
  puts "New release commit was successful."
else
  abort "error: could not execute command - #{output}"
end

# Push new branch to remote
output = `git push #{remote} 2>&1`
if $?.success?
  puts "Push of new branch to remote #{remote} was successful."
else
  abort "error: could not execute command - #{output}"
end

# Create new PR based on new branch
new_PR_url = URI.extract(output).select {|token| token.start_with?("https://")}[0]
puts "Opening Github new PR creation page."
`open #{new_PR_url}`

# Clean up.
puts "Tidying up branches."
`git checkout release_script && git branch -D new_cbmc_release-#{new_version}`

puts "First part of release completed successfully."
