<?php

/*
 * This file is part of the Daw package.
 *
 * (c) Axel Tessier <axel.tessier@extendev.net>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

namespace Daw\Core\Service;

class ServiceResponse {
	protected $request = null;

	public $success;
	public $code;
	public $message;

	public $contents;

	public function setRequest($request) {
		$this->request = $request;
	}
}