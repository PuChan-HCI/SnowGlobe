#!/usr/bin/env python3
import numpy
import cv2
import logging

def get_frame(cap):
	ret, image = cap.read()
	return cv2.cvtColor(image,cv2.COLOR_BGR2GRAY)	

# Average a specified number of frames to mitigate noise
def get_average_image(cap, num_frames):
	image = get_frame(cap)

	image = numpy.float32(image)/255.0
	average = image.copy()

	for i in range(num_frames):
	 	ret, image = cap.read()
	 	image = cv2.cvtColor(image,cv2.COLOR_BGR2GRAY)

	 	image = numpy.float32(image)/255.0
	 	cv2.accumulateWeighted(image, average, 1.0/num_frames)

	average = numpy.uint8(average*255.0)
	return average

# Create blob detector parameters hand tuned for this use case
def get_detector_params():
	params = cv2.SimpleBlobDetector_Params()

	params.filterByCircularity = True
	params.minCircularity = 0.4
	logging.debug("Circularity %d %d", params.minCircularity, params.maxCircularity)

	params.filterByConvexity = True
	params.minConvexity = 0.4
	logging.debug("Convexity %d %d", params.minConvexity, params.maxConvexity)

	params.filterByInertia = True
	logging.debug("Inertia %d %d", params.minInertiaRatio, params.maxInertiaRatio)

	params.filterByArea = True
	params.minArea = 10
	params.maxArea = 500
	logging.debug("Area %d %d", params.minArea, params.maxArea)

	params.minDistBetweenBlobs = 3.0
	logging.debug("minDistBetweenBlobs %d", params.minDistBetweenBlobs)

	params.minRepeatability = 2
	logging.debug("minRepeatability %d", params.minRepeatability)

	params.blobColor = 255
	params.minThreshold = 10
	params.maxThreshold = 60
	params.thresholdStep = 5
	logging.debug("Threshold %d %d %d", params.minThreshold, params.maxThreshold, params.thresholdStep)

	return params

if __name__ == "__main__":

	logging.basicConfig(level=logging.DEBUG)

	cap = cv2.VideoCapture(0)
	# The Ailipu camera I used runs fastest at VGA
	#cap.set(cv2.CAP_PROP_FOURCC , cv2.VideoWriter_fourcc(*'MJPG'))
	cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
	cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

	# Flush some frames
	for i in range(10):
		get_frame(cap)

	average = get_average_image(cap, 10)

	# Create a blob detector
	params = get_detector_params()
	detector = cv2.SimpleBlobDetector_create(params)

	tick = cv2.getTickCount()

	while(True):
	    # Capture frame-by-frame
	    frame = get_frame(cap)
	    frame = cv2.subtract(frame, average)

	    # Update the blob detector
	    keypoints = detector.detect(frame)
	    frame = cv2.drawKeypoints(frame, keypoints, numpy.array([]), (0,0,255), cv2.DRAW_MATCHES_FLAGS_DRAW_RICH_KEYPOINTS)

	    # Display the resulting frame
	    cv2.imshow('frame', frame)

	    # Time the loop
	    new_tick = cv2.getTickCount()
	    t = (new_tick-tick)/cv2.getTickFrequency()
	    tick = new_tick
	    logging.debug("t {:05.3f} fps {:05.2f}".format(t, 1.0/t))

	    if cv2.waitKey(1) & 0xFF == ord('q'):
	        break

	# When everything done, release the capture
	cap.release()
	cv2.destroyAllWindows()
