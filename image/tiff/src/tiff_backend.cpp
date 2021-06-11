#include "../include/tiff_backend.hpp"

#include "../include/tiff_template.hpp"

#include <thread>

namespace Image {

	TIFFBackend::TIFFBackend(): ImageBackendImpl(), pImpl(nullptr) {}

	bool TIFFBackend::canReadImage(const std::string& image_name) {
		// Check the file ends with a known and good extension, and then check we can open it.
		std::size_t extPos = image_name.find_last_of(".");
		// if no extension is present, the file type is unknown, return false :
		if (extPos == std::string::npos) { return false; }

		// Extract the extension from the name :
		std::string ext = image_name.substr(extPos, std::string::npos);

		#warning All file extensions of TIFF are not specified here.
		// If the extension is known, return true !
		if (ext == ".tif" || ext == ".tiff") {
			// If the extension is valid, then try to parse the first frame of the file :
			try {
				#warning Doesnt take into account the templated backends of this class.
				Tiff::Frame::Ptr test_frame = std::make_shared<Tiff::Frame>(image_name.data(), 0);
				return true;
			} catch (std::exception _e) {
				#warning Should we alert the user here ?
				return false;
			}
		}

		// Otherwise, return false
		return false;
	}

	ImageBackendImpl::Ptr TIFFBackend::createBackend() {
		return ImageBackendImpl::Ptr(new TIFFBackend());
	}

	ThreadedTask::Ptr TIFFBackend::parseImageInfo(ThreadedTask::Ptr pre_existing_task, const std::vector<std::vector<std::string>>& _filenames) {
		/* Parses the image in a separate thread. Detaches it from the main thread, in order to
		 * let it free up its own resources. */
		if (pre_existing_task == nullptr) { pre_existing_task = std::make_shared<ThreadedTask>(); }

		// If no filenames are provided, just return and end the task :
		if (_filenames.empty()) { pre_existing_task ->end(); return pre_existing_task ; }

		// Launch the parsing of files in a separate thread :
		std::thread parseThread = std::thread(&TIFFBackend::parseImageInfo_thread,
											  this, pre_existing_task, std::ref(_filenames));

		// Wait for the task to be initialized, in 5ms increments :
		while (pre_existing_task->getMaxSteps() == 0 && not pre_existing_task->isComplete()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		// Detach the thread, allowing it to continue executing outside of this scope :
		parseThread.detach();

		return pre_existing_task ;
	}

	ImageDataType TIFFBackend::getInternalDataType() const {
		if (this->pImpl) { return this->pImpl->getInternalType(); }
		return ImageDataType::Unknown;
	}

	bool TIFFBackend::presentOnDisk() const {
		if (this->pImpl) { return true; }
		return false;
	}

	std::string TIFFBackend::getImageName() const {
		return "tiff_backend_implementation";
	}

	void TIFFBackend::setImageName(std::string &_user_defined_name_) {
		UNUSED_PARAMETER(_user_defined_name_);
		return;
	}

	BoundingBox_General<float> TIFFBackend::getBoundingBox() const {
		return Image::bbox_t(glm::vec3(.0f, .0f, .0f), glm::convert_to<float>(this->getResolution()));
	}

	std::size_t TIFFBackend::getVoxelDimensionality() const {
		return this->voxel_dimensionality;
	}

	glm::vec3 TIFFBackend::getVoxelDimensions() const {
		if (this->pImpl) { return this->pImpl->getVoxelDimensions(); }
		else return glm::vec3(.0f, .0f, .0f);
	}

	void TIFFBackend::parseImageInfo_thread(ThreadedTask::Ptr task, const std::vector<std::vector<std::string>>& _filenames) {
		// IF no filenames, return and end task :
		if (_filenames.empty()) {
			task->pushMessage("Filenames were empty.");
			task->end(false);
			return;
		}

		/**
		 * Steps to perform :
		 *		- Initialize reference frame to be the first frame of the first file
		 *		- For all vectors of filenames :
		 *			- For all files within it :
		 *				- For all frames within those files :
		 *					- Check theya re compatible in bitwidths with the main file
		 */

		if (this->preprocessFilenames(task, _filenames) == false) {
			task->end(false);
			return;
		}

		// Try to allocate and parse reference frame :
		Tiff::Frame::Ptr reference_frame_main;
		try {
			reference_frame_main = std::make_shared<Tiff::Frame>(_filenames[0][0], 0);
		}  catch (std::runtime_error _e) {
			task->pushMessage(std::string("Could not parse files (reference frame was not properly parsed).\n"
										  "Error message from TIFF backend : ")+_e.what());
			task->end(false);
			return;
		}

		try {
			// Allocate the right backend for this task :
			this->createTiffBackend(reference_frame_main, _filenames.size());
		} catch (std::runtime_error _e) {
			task->pushMessage(std::string("Error while creating TIFF reading backend.\nError message : ")+_e.what());
			task->end(false);
			return;
		}

		// Get some info about the reference frame :
		TIFF* reference_handle = reference_frame_main->getLibraryHandle();
		uint32_t w = reference_frame_main->width(reference_handle);
		uint32_t h = reference_frame_main->height(reference_handle);
		uint16_t bps = reference_frame_main->bitsPerSample(reference_handle);
		TIFFClose(reference_handle);

		// We can already set the dimensionality of the dataset :
		this->voxel_dimensionality = _filenames.size();
		// Since checkFilenamesAreValid() returned true, the # of frames (Z-depth) is set into task->maxSteps !
		// We can thus already set the image resolution here :
		this->resolution = svec3(w, h, task->getMaxSteps());
		// We can also set some other known data here :
		this->voxel_dimensions = glm::vec3(1.f, 1.f, 1.f);
		this->internal_data_type = this->pImpl->getInternalType();

		// Iterate on all filenames, extract frames :
		for (std::size_t name_it = 0; name_it < _filenames[0].size(); ++name_it) {
			// Number of frames inside the current file :
			tsize_t dirSize = Tiff::countDirectories(_filenames[0][name_it]);

			std::cerr << "Iterating over " << dirSize << " directories.\n";

			// Iterate on all frames :
			for (tdir_t fr_it = 0; fr_it < dirSize; ++fr_it) {
				// will hold all loaded and parsed frames :
				Tiff::TIFFPrivate::TIFFImage imgframes;

				// For all color channels :
				for (std::size_t c_it = 0; c_it < this->voxel_dimensionality; ++c_it) {
					try {
						// Create and push back a new frame of this filename, at IFD index 'fr_it' :
						Tiff::Frame::Ptr fr = std::make_shared<Tiff::Frame>(_filenames[c_it][name_it], fr_it);
						// If the frame's not compatible, return error to the user :
						if (fr->isCompatibleWith(w, h, bps) == false) {
							std::string err = "WARNING: Frame "+std::to_string(fr_it)+", ch. "+std::to_string(c_it)+
											" does not have the same resolution as the reference frames. Stopping ...";
							task->pushMessage(err);
							imgframes.clear();
							task->end();
							return;
						} else {
							// add the frame :
							imgframes.push_back(fr);
						}
					} catch(std::runtime_error _e) {
						// IF the frame's creation triggered a runtime error, stop and return :
						task->pushMessage(std::string("Error : could not create frame " + std::to_string(fr_it) +
										  " for channel " + std::to_string(c_it) + ".\nError message :")+_e.what());
						task->end();
						return;
					}
				}
				task->advance();

				// Add newly created image to the implementation :
				this->pImpl->addImage(imgframes);
			}
		}

		auto res = this->getResolution();
		std::cerr << "Finished parsing, resolution : [" << res.x << ", " << res.y << ", " << res.z << "]\n";

		task->end();
		// exit the thread :
		return;
	}

	svec3 TIFFBackend::getResolution() const {
		if (this->pImpl) { return this->pImpl->getResolution(); }
		return svec3(0,0,0);
	}

	void TIFFBackend::createTiffBackend(Tiff::Frame::Ptr reference_frame, std::size_t _dim) {
		// get the file handle by libtiff :
		TIFF* f = reference_frame->getLibraryHandle();
		// get the number of bits per pixel :
		uint16_t bps = reference_frame->bitsPerSample(f);
		// get frame information :
		uint32_t w = reference_frame->width(f);
		uint32_t h = reference_frame->height(f);
		uint16_t sf = reference_frame->sampleFormat(f);
		TIFFClose(f);

		switch (sf) {
			case SAMPLEFORMAT_VOID:
				throw std::runtime_error("Internal type of the frame was void.");
			break;

			case SAMPLEFORMAT_UINT: {
				if (bps == 8) { this->pImpl = Tiff::TIFFReader<std::uint8_t>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 16) { this->pImpl = Tiff::TIFFReader<std::uint16_t>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 32) { this->pImpl = Tiff::TIFFReader<std::uint32_t>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 64) { this->pImpl = Tiff::TIFFReader<std::uint64_t>::createTIFFBackend(w, h, _dim); return; }
				std::string err = "Sample was UInt, but no matching ctor was found for "+std::to_string(bps) + " bits.";
				throw std::runtime_error(err);
			}
			break;

			case SAMPLEFORMAT_INT: {
				if (bps == 8) { this->pImpl = Tiff::TIFFReader<std::int8_t>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 16) { this->pImpl = Tiff::TIFFReader<std::int16_t>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 32) { this->pImpl = Tiff::TIFFReader<std::int32_t>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 64) { this->pImpl = Tiff::TIFFReader<std::int64_t>::createTIFFBackend(w, h, _dim); return; }
				std::string err = "Sample was Int, but no matching ctor was found for "+std::to_string(bps) + " bits.";
				throw std::runtime_error(err);
			}
			break;

			case SAMPLEFORMAT_IEEEFP: {
				if (bps == 32) { this->pImpl = Tiff::TIFFReader<float>::createTIFFBackend(w, h, _dim); return; }
				if (bps == 64) { this->pImpl = Tiff::TIFFReader<double>::createTIFFBackend(w, h, _dim); return; }
				std::string err = "Sample was floating point , but no matching ctor was found for "+std::to_string(bps)
								+ " bits.";
				throw std::runtime_error(err);
			}
			break;

			case SAMPLEFORMAT_COMPLEXINT:
				throw std::runtime_error("The internal type was complex (not supported).");
			break;

			case SAMPLEFORMAT_COMPLEXIEEEFP:
				throw std::runtime_error("The internal type was complex (not supported).");
			break;

			default:
				throw std::runtime_error("The internal type was not recognized (not in libTIFF's types).");
			break;
		}
	}

	bool TIFFBackend::preprocessFilenames(ThreadedTask::Ptr& task, const std::vector<std::vector<std::string>>& _filenames) const {
		// We first have to check there are the same number of files in each component :
		std::size_t ref_file_count = _filenames[0].size();
		for (std::size_t i = 0; i < _filenames.size(); ++i) {
			if (_filenames[i].size() != ref_file_count) {
				std::string err = "Component " + std::to_string(i) + " : expected " +
									std::to_string(ref_file_count) + " files, but got " +
									std::to_string(_filenames[i].size()) + " instead.";
				task->pushMessage(err);
				return false;
			}
		}

		// Compute the number of directories to parse !
		tdir_t total_dir_count = 0;

		// Then, for each file in the components, we have to check it has the same # of directories :
		for (std::size_t i = 0; i < _filenames[0].size(); ++i) {
			// ref for component 0 :
			tdir_t ref_frame_count = Tiff::countDirectories(_filenames[0][i]);
			for (tdir_t j = 0; j < _filenames.size();	++j) {
				tdir_t cur_frame_count = Tiff::countDirectories(_filenames[j][i]);
				if (cur_frame_count != ref_frame_count) {
					std::string err = "Error : file \"" + _filenames[j][i] + " contained " +
										std::to_string(cur_frame_count) + " instead of the " +
										std::to_string(ref_frame_count) + " expected.";
					task->pushMessage(err);
					return false;
				}
			}
			total_dir_count += ref_frame_count;
		}

		// This stack now has :
		// the same number of files in each component
		// the same number of TIFF directories in each component (per-slice)
		// we can just return at this point.
		task->setSteps(total_dir_count);
		return true;
	}

	void TIFFBackend::internal_cleanup_after_error() {
		this->voxel_dimensions = glm::vec3(-1.f, -1.f, -1.f);
		this->resolution = svec3(0, 0, 0);
		this->internal_data_type = ImageDataType::Unknown;
		this->pImpl = nullptr;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes.  8-bit unsigned version.
	bool TIFFBackend::internal_readSubRegion(tag<std::uint8_t> tag,
		svec3 origin, svec3 size, std::vector<std::uint8_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes. 16-bit unsigned version.
	bool TIFFBackend::internal_readSubRegion(tag<std::uint16_t> tag,
		svec3 origin, svec3 size, std::vector<std::uint16_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes. 32-bit unsigned version.
	bool TIFFBackend::internal_readSubRegion(tag<std::uint32_t> tag,
		svec3 origin, svec3 size, std::vector<std::uint32_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes. 64-bit unsigned version.
	bool TIFFBackend::internal_readSubRegion(tag<std::uint64_t> tag,
		svec3 origin, svec3 size, std::vector<std::uint64_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes.  8-bit   signed version.
	bool TIFFBackend::internal_readSubRegion(tag<std::int8_t> tag,
		svec3 origin, svec3 size, std::vector<std::int8_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes. 16-bit   signed version.
	bool TIFFBackend::internal_readSubRegion(tag<std::int16_t> tag,
		svec3 origin, svec3 size, std::vector<std::int16_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes. 32-bit   signed version.
	bool TIFFBackend::internal_readSubRegion(tag<std::int32_t> tag,
		svec3 origin, svec3 size, std::vector<std::int32_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes. 64-bit   signed version.
	bool TIFFBackend::internal_readSubRegion(tag<std::int64_t> tag,
		svec3 origin, svec3 size, std::vector<std::int64_t>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes, single precision floating point.
	bool TIFFBackend::internal_readSubRegion(tag<float> tag,
		svec3 origin, svec3 size, std::vector<float>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	/// @b Read a sub-region of the image, implemented in the derived classes, double precision floating point.
	bool TIFFBackend::internal_readSubRegion(tag<double> tag,
		svec3 origin, svec3 size, std::vector<double>& data)
	{
		PRINT_FN_ENTRY;
		if (this->pImpl) { return this->pImpl->tiff_readSubRegion(tag, origin, size, data); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::int8_t> tag, std::size_t channel,
											  glm::vec<2, std::int8_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::int16_t> tag, std::size_t channel,
											  glm::vec<2, std::int16_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::int32_t> tag, std::size_t channel,
											  glm::vec<2, std::int32_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::int64_t> tag, std::size_t channel,
											  glm::vec<2, std::int64_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::uint8_t> tag, std::size_t channel,
											  glm::vec<2, std::uint8_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::uint16_t> tag, std::size_t channel,
											  glm::vec<2, std::uint16_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::uint32_t> tag, std::size_t channel,
											  glm::vec<2, std::uint32_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<std::uint64_t> tag, std::size_t channel,
											  glm::vec<2, std::uint64_t, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<float> tag, std::size_t channel,
											  glm::vec<2, float, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

	bool TIFFBackend::internal_getRangeValues(tag<double> tag, std::size_t channel,
											  glm::vec<2, double, glm::defaultp> &_values)
	{
		if (this->pImpl) { return this->pImpl->tiff_getRangeSubValues(tag, channel, _values); }
		return false;
	}

}
