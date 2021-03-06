#!/usr/bin/env ruby

require 'fileutils'
require 'openssl'

class BuildZip

  def self.aes_encrypt(clear_text, key, padding = 1)
    cipher = OpenSSL::Cipher::Cipher.new('aes-128-cbc')
    cipher.encrypt
    cipher.padding = padding
    cipher.key = key
    cipher.iv = "\x00" * cipher.iv_len
    edata = cipher.update(clear_text)
    edata << cipher.final
    return edata
  end

  def self.aes_decrypt(enc_text, key, padding = 1)
    decipher = OpenSSL::Cipher::Cipher.new('aes-128-cbc')
    decipher.decrypt
    decipher.padding = padding
    decipher.key = key
    decipher.iv = "\x00" * decipher.iv_len
    data = decipher.update(enc_text)
    data << decipher.final
    return data
  end

  def self.run

    # remove any previous zip file
    Dir['ht*.zip'].each do |file|
      FileUtils.rm_rf file
      FileUtils.rm_rf File.basename(file, '.zip')
    end
    Dir['ht-????-???'].each do |file|
      FileUtils.rm_rf file
    end

    # create the zip files
    Dir['ht*'].each do |dir|
      next unless File.directory? dir
      next if dir.start_with? '.'

      zipfile = dir[0..(dir.rindex('-')-1)]
      puts zipfile

      Dir.chdir dir
      system "zip -r ../#{zipfile}.zip *"
      Dir.chdir '..'
    end

    # encrypt them
    Dir['ht*.zip'].each do |file|
      content = File.binread(file)
      crypt = aes_encrypt(content, "$RCS Galileo Exploit$")
      File.open(File.basename(file, ".zip"), "wb") {|f| f.write crypt}
      FileUtils.rm_rf file
    end

    # check decryption
    Dir['ht-????-???'].each do |exp|
      clear_file = File.basename(exp) + '.zip'
      content = aes_decrypt(File.binread(exp), "$RCS Galileo Exploit$")
      File.open(clear_file, 'wb') {|f| f.write content}
      FileUtils.rm_rf clear_file
    end

  end

end

if __FILE__ == $0
  BuildZip.run
end