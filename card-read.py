import time

from pirc522 import RFID
rdr = RFID()

rdr.wait_for_tag()
(error, tag_type) = rdr.request()
if not error:
  print("Tag detected")
  (error, uid) = rdr.anticoll()
  if not error:
    print("UID: " + str(uid))
    # Select Tag is required before Auth
    if not rdr.select_tag(uid):
      # Auth for block 10 (block 2 of sector 2) using default shipping key A
      if not rdr.card_auth(rdr.auth_a, 10, [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF], uid):
        # This will print something like (False, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        for num in range(1,63):
          print("reading "+str(num)+" block data: \t "+str(rdr.read(num)))
          time.sleep(0.1)
        # Always stop crypto1 when done working
        rdr.stop_crypto()

# Calls GPIO cleanup
rdr.cleanup()
