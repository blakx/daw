<?php

/**
 * This file is part of the Daw package.
 *
 * (c) Axel Tessier <axel.tessier@extendev.net>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

namespace Daw\Core\Module;

use \Daw\Core\Common\Json;
use \Daw\Core\Exception\OperationArgumentException;
use \Daw\Core\Module\Operation\Argument\UpdateList;

/**
 * Application Module - Operation
 *
 * Every operation to be included in modules shall extend
 * this class to be supported by the application
 *
 * @package Daw
 * @subpackage Module
 * @author Axel Tessier <axel.tessier@extendev.net>
 * @version $Id: Event.inc 112 2003-08-22 06:22:40Z  $
 */
abstract class Operation {

    /**
     * Core instance
     *
     * @var \Daw\Core\Core
     */
    protected $core = array();

    /**
     * @var Module Parent module
     */
    protected $module = array();

    /**
     * Operation information extracted by Daw module
     * and extracted from module.info file
     *
     * @var array operation info
     */
    protected $info = array();

    /**
     * @var Module Parent module
     */
    protected $args = array();


    /**
     * Constructor
     *
     * @param Module $module
     * @param array operation info
     */
    public function __construct($module, $info) {
        $this->module = $module;
        $this->info = $info;

        $this->core = $module->core();
    }

    /**
     * Base validation method
     *
     * This validates all arguments based on the validation specified
     * in the operation description
     *
     * @param array $args
     * @throws OperationArgumentException
     */
    protected function validate($args) {
        if (!isset($this->info['args']) || !$this->info['args']) {
            return true;
        }

        foreach ($this->info['args'] as $argName => $argInfo) {

            if ($this->isArgSet($argName) && isset($argInfo['format'])) {
                $this->validateFormat($argName, $this->getArg($argName, false), $argInfo['format'], isset($argInfo['options']) ? $argInfo['options'] : null, isset($argInfo['format-msg']) ? $argInfo['format-msg'] : null);
            }

            // Check if validation has been specified for this argument
            if (!isset($argInfo['validation'])) {
                continue;
            }

            // Check if user specified validation as a single string element or as an array
            $validationRules = $argInfo['validation'];
            if (!is_array($validationRules)) {
                $validationRules = explode(' ', $validationRules);
            }

            // Now process each rule
            foreach ($validationRules as $rule) {

                // A validation rule may use following format
                //   function           when validation is handled by function defined in the application
                //   Object::function   when validation is defined in an object
                switch ($rule) {
                    case 'required':
                        if (!$this->isArgSet($argName)) {
                            throw new OperationArgumentException($this, $argName, 'Value is required');
                        }
                        break;
                    default:
                        $matches = array();
                        if (preg_match_all("/^([^:]*)([:]+)([^:]*)$/", $rule, $matches) === 0) {
                            throw new OperationArgumentException($this, $argName, 'Invalid validation rule: ' . $rule);
                        }

                        $object = current($matches[1]);
                        $operator = current($matches[2]);
                        $params = current($matches[3]);

                        switch ($operator) {
                            case ':':
                                $value = $this->getArg($argName);
                                if ($value instanceof UpdateList) {
                                    foreach ($value->set as $val) {
                                        if (!empty($val)) {
                                            if (!$this->core->getObjectStore($object)->exists(array($params => $val))) {
                                                throw new OperationArgumentException($this, $argName, 'Object does not exist', $val);
                                            }
                                        }
                                    }
                                    foreach ($value->added as $val) {
                                        if (!empty($val)) {
                                            if (!$this->core->getObjectStore($object)->exists(array($params => $val))) {
                                                throw new OperationArgumentException($this, $argName, 'Object does not exist', $val);
                                            }
                                        }
                                    }
                                    foreach ($value->removed as $val) {
                                        if (!empty($val)) {
                                            if (!$this->core->getObjectStore($object)->exists(array($params => $val))) {
                                                throw new OperationArgumentException($this, $argName, 'Object does not exist', $val);
                                            }
                                        }
                                    }
                                } else {
                                    if ($this->isArgSet($argName) && !$this->core->getObjectStore($object)->exists(array($params => $this->getArg($argName)))) {
                                        throw new OperationArgumentException($this, $argName, 'Object does not exist', $this->getArg($argName));
                                    }
                                }
                                break;
                            case '::':
                                if (!method_exists($object, $params)) {
                                    throw new OperationArgumentException($this, $argName, 'Invalid validation rule, requested method does not exist: ' . $rule);
                                }

                                // Run object's validation method and catch ValidationException in case onr validation fails
                                try {
                                    call_user_method($params, new $object(), $this->getArg($argName));
                                } catch (ValidationException $ve) {
                                    throw new OperationArgumentException($this, $argName, $ve->getMessage(), $this->getArg($argName));
                                }
                                break;
                        }
                        break;
                }
            }
        }
    }

    /**
    * Validate an argument against a given format
    * Autovalidation may be performed if module owner specified in Module's module.info description
    * of the operation different information for the argument like its format or allowed options.
    *
    * You may want to override this method to add specific validation for your arguments
    *
    * @param string $argName name of the argument
    * @param object $arg value of the argument
    * @param string $format format defined in operation description
    * @param array $formatOptions array possible options allowed - if set, the argument shall be set with one of them
    * @param string $formatHint hint associated to the argument - can be used as publishable information
    */
    protected function validateFormat($argName, $arg, $format, $formatOptions = array(), $formatHint = null) {

        // Check format
        if (preg_match_all("/^([^(]*)(?:\((.*)\))?+/", $format, $matches) === 0) {
            throw new OperationArgumentException($this, $argName, 'Invalid validation rule format: ' . $format);
        }

        $baseFormat = array_shift($matches[1]);
        $formatArgs = explode(",", array_shift($matches[2]));

        switch ($baseFormat) {
            case 'boolean':
                if (filter_var($arg, FILTER_VALIDATE_BOOLEAN) === false && !in_array($arg, array('0', '1'))) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : 'Invalid format, possible options: true, false, 1, 0.', $arg);
                }
                break;

            case 'date':
                $dateFormat = count($formatArgs) > 0 ? $formatArgs[0] : 'Ymd';
                if (\DateTime::createFromFormat($dateFormat, $arg) === false) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : ('Date format is invalid, expected format is: ' . $dateFormat), $arg);
                }
                break;

            case 'ip':
                if (filter_var($arg, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false
                    && filter_var($arg, FILTER_VALIDATE_IP, FILTER_FLAG_IPV6) === false) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : 'Invalid IPv4 address, format expected is xxx.xxx.xxx.xxx.', $arg);
                }
                break;

            case 'ipv4':
                if (filter_var($arg, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : 'Invalid IPv4 address, format expected is xxx.xxx.xxx.xxx.', $arg);
                }
                break;

            case 'ipv6':
                if (filter_var($arg, FILTER_VALIDATE_IP, FILTER_FLAG_IPV6) === false) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : 'Invalid IPv6 address.', $arg);
                }
                break;

            case 'url':
                if (filter_var($arg, FILTER_VALIDATE_URL) === false) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : 'Invalid URL', $arg);
                }
                break;

            case 'fqdn':
                if (filter_var($arg, FILTER_VALIDATE_FQDN) === false) {
                    throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : 'Invalid FQDN', $arg);
                }
                break;

            case 'list':
                // extract list from arg
                $list = Json::decode($arg);
                if (!is_array($list)) {
                    $list = explode("\n", $arg);
                }

                $tokenFormat = count($formatArgs) ? array_shift($formatArgs) : null;
                foreach ($list as $idx => $value) {
                    if (in_array($value[0], array('-', '+'))) {
                        $value = substr($value, 1);
                    }
                    $this->validateFormat($argName . '[' . $idx . ']', $value, $tokenFormat, $formatOptions, $formatHint);
                }
                break;

            case 'number':
                break;

            default:
                // Check options
                if (isset($formatOptions) && is_array($formatOptions) && count($formatOptions)) {
                    if (!in_array($arg, $formatOptions)) {
                        throw new OperationArgumentException($this, $argName, $formatHint !== null ? $formatHint : ('Value "' . $arg . '" must be one of the following options: ' . implode(', ', $formatOptions)));
                    }
                }
                break;
        }

    }

    /**
     * Get the list of args passed to the operation
     *
     * @return array arguments
     */
    protected function getArgs() {
        return $this->args;
    }

    /**
     * Check if argument has been set, but not necessarly not empty.
     *
     * Note that the value may be empty. If you want to check if the value
     * is not empy too, use isArgEmpty()
     *
     * @param string $name
     */
    protected function isArgSet($name) {
        return isset($this->args[$name]) && $this->args[$name] !== false;
    }

    /**
     * Check if argument has been set, and that value is not empty
     *
     * @param string $name
     */
    protected function isArgEmpty($name) {
        return $this->isArgSet($name) && strlen(trim($this->getArg($name, false))) !== 0;
    }

    /**
     * Return argument
     *
     * @param string $name
     * @param boolean $strict specify true to get argument strictly typed
     */
    protected function getArg($name, $strict = true) {
        if (!isset($this->args[$name])) {
            return null;
        }

        $value = $this->args[$name];
        if ($strict) {
            $argInfo = $this->info['args'][$name];
            $format = isset($argInfo['format']) ? $argInfo['format'] : null;
            if (preg_match_all("/^([^(]*)(?:\((.*)\))?+/", $format, $matches) === 0) {
                throw new OperationArgumentException($this, $argName, 'Invalid validation rule format: ' . $format);
            }

            $baseFormat = array_shift($matches[1]);
            $formatArgs = explode(",", array_shift($matches[2]));
            switch ($baseFormat) {
                case 'boolean':
                    switch ($value) {
                        case '1':
                        case 'true':
                            return true;
                            break;
                        case '0':
                        case 'false':
                            return false;
                            break;
                    }
                    $dateFormat = count($formatArgs) > 0 ? $formatArgs[0] : 'Ymd';
                    $value = \DateTime::createFromFormat($dateFormat, $value);
                    break;
                case 'date':
                    $dateFormat = count($formatArgs) > 0 ? $formatArgs[0] : 'Ymd';
                    $value = \DateTime::createFromFormat($dateFormat, $value);
                    break;
                case 'map':
                    // extract map from arg
                case 'list':
                    // extract list from arg
                    $list = Json::decode($value);
                    if (!is_array($list)) {
                        $list = !empty($value) ? explode("\n", $value) : array();
                    }
                    $value = new UpdateList($list);
                    break;
                case 'integer':
                    $value = (int)$value;
                    break;
            }
        }

        return $value;
    }

    /**
     * Call operation
     * This method performs validation by calling validate()
     * and if no violation has been found calls run()
     *
     * @param array $args arguments specified for this operation
     * @return object the response of the operation
     */
    public function call($args = array()) {
        $this->args = $args;
        $this->validate($this->args);

        return $this->run($args);
    }

    /**
     * Return pre-formatted message for success case
     * This method is called by the run() method if the operation completed successfully.
     *
     * @param string $message message to be displayed to the user
     * @param array $data additionnal data that may have been specified by the operation when calling the method and to be included in the response.
     * @return array
     */
    protected function success($message = 'Operation completed successfully', $data = null) {
        $msg = array(
            'success' => true,
            'message' => $message
        );

        if ($data !== null) {
            $msg['data'] = $data;
        }

        return $msg;
    }

    /**
     * Return pre-formatted message for failure case
     * This method is called by the run() method if the operation failed to complete.
     * It may include a code to identify a type of error and additional data to understand the context of the failure.
     *
     * @param string $message message to be displayed to the user
     * @param string $code code associated to the error
     * @param array $data additionnal data that may have been specified by the operation when calling the method and to be included in the response.
     * @return array
     */
    protected function failure($message = 'Operation failed', $code = null, $data = null) {
        $msg = array(
            'success' => false,
            'message' => $message
        );

        if ($code !== null) {
            $msg['code'] = $code;
        }

        if ($data !== null) {
            $msg['data'] = $data;
        }

        return $msg;
    }

    /**
     * Run the operation
     * This method may return data to be returned to the user upon successful completion of the operation
     *
     * @param $args arguments
     * @return object response
     */
    public abstract function run($args = array());
}
