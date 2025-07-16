# ZoZoCopy

have you ever wanted to copy files on an ext4 drive whilst preserving the timestamps? well, i sure did, but every fourum on the internet i could find said that was entirely impossible (unless you use macOS)....
but instead of giving up , i just took that as a challange to make a tool myself!!!!
introducing... zozocopy! 

a tool for unix that copies a directory to a desintation, and ensures the copied versions files all have the correct dates created
made to preserve date created when transfering between a ntfs system to an ext4 sytstem, windows to linux

yes i have tried cp -a and rsync.
