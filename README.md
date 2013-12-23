Arduino-Nimbits
===============

This has an updated Arduino Library for Nimbits IOT.

This version adds 3 functions to the Nimbits-Arduino lib:
getValue
getSeries
getTime

getValue will return the most recent vlaue stored in the Nimbits cload for a data point.

get Series will return upto 10 values with the human readable time stamp

getTime will return the Nimbits server Unix Time.  Good for checking connectivity and syncing the time to your arduino.

