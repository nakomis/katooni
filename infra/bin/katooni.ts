#!/usr/bin/env node
import * as fs from 'fs';
import * as path from 'path';
import * as cdk from 'aws-cdk-lib';
import { DeviceStack } from '../lib/device-stack';

const npmEnvironment = process.env.NPM_ENVIRONMENT;
if (npmEnvironment !== 'sandbox' && npmEnvironment !== 'prod') {
  throw new Error('NPM_ENVIRONMENT must be "sandbox" or "prod". Use `pnpm run deploy-sandbox`.');
}
const deployEnv = npmEnvironment;

// The account comes from the profile the deploy runs under, never a literal.
const env = { account: process.env.CDK_DEFAULT_ACCOUNT, region: process.env.CDK_DEFAULT_REGION ?? 'eu-west-2' };

const app = new cdk.App();

// One IoT Thing per physical Katooni, listed per environment in infra/devices.json. Committed, so
// every deploy declares every device: one left out of a deploy would have its Thing deleted.
const deviceNames: string[] = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'devices.json'), 'utf8'))[deployEnv];
if (!Array.isArray(deviceNames) || deviceNames.some((d) => !/^unit-[0-9]+$/.test(d)) || new Set(deviceNames).size !== deviceNames.length) {
  throw new Error(`infra/devices.json: "${deployEnv}" must be a list of distinct names like unit-1`);
}

new DeviceStack(app, 'KatooniDeviceStack', {
  env,
  deployEnv,
  deviceNames,
  description: `Katooni cable testers as IoT Things, for Fleet OTA (${deployEnv})`,
});

const { version: infraVersion } = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'version.json'), 'utf8'));
cdk.Tags.of(app).add('MH-Project', 'katooni');
cdk.Tags.of(app).add('MH-Version', infraVersion);
cdk.Tags.of(app).add('Project', 'katooni');
